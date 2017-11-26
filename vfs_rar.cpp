#include <cassert>
#include <vector>
#include <string>
using namespace std;

#include <deadbeef/deadbeef.h>
#include "unrar/rar.hpp"

//-----------------------------------------------------------------------------

#ifdef DEBUG
#define trace(...) { fprintf(stderr, __VA_ARGS__); }
#else
#define trace(fmt,...)
#endif

#define min(x,y) ((x)<(y)?(x):(y))

static DB_functions_t *deadbeef;
static DB_vfs_t plugin;

typedef struct {
	DB_FILE file;

	Archive *arc;
	ComprDataIO *dataIO;
	Unpack *unp;
	byte *buffer;
	size_t buffer_size;

	int64_t offset;
	int64_t size;
} rar_file_t;

static const char *scheme_names[] = { "rar://", NULL };

void unstore_file(ComprDataIO *dataIO, int64_t size)
{
	byte buffer[0x10000];
	while (1) {
		uint code = dataIO->UnpRead(buffer, 0x10000);
		if (code == 0 || (int)code == -1)
			break;
		code = min(code, size);
		dataIO->UnpWrite(buffer, code);
		if (size >= 0)
			size -= code;
	}
}

//-----------------------------------------------------------------------------

const char **
vfs_rar_get_schemes (void)
{
	return scheme_names;
}

int
vfs_rar_is_streaming (void)
{
	return 0;
}

// fname must have form of rar://full_filepath.rar:full_filepath_in_rar
DB_FILE*
vfs_rar_open (const char *fname)
{
	trace("[vfs_rar_open] %s\n", fname);
	if (strncasecmp (fname, "rar://", 6))
		return NULL;

	// get the full path of .rar file
	fname += 6;
	const char *colon = strchr(fname, ':');
	if (!colon) {
		return NULL;
	}

	char rarname[colon-fname+1];
	memcpy(rarname, fname, colon-fname);
	rarname[colon-fname] = '\0';

	// get the compressed file in this archive
	fname = colon+1;

	wchar_t wrarname[NM];
	UtfToWide(rarname, wrarname, ASIZE(wrarname)-1);

	wchar_t wfname[NM];
	UtfToWide(fname, wfname, ASIZE(wfname)-1);

	Archive *arc = new Archive();
	trace("opening rar file: %s\n", rarname);
	if (!arc->WOpen(wrarname))
		return NULL;

	if (!arc->IsArchive(true))
		return NULL;

	// find the desired file from archive
	trace("searching file %s\n", fname);
	bool found_file = false;
	while (arc->ReadHeader() > 0) {
		int hdr_type = arc->GetHeaderType();
		if (hdr_type == HEAD_ENDARC)
			break;

		switch (hdr_type) {
		case HEAD_FILE:
			if (!arc->IsArcDir()) {
				wchar_t warcfn[NM];
				ConvertPath(arc->FileHead.FileName, warcfn);
				if (!wcscmp(warcfn, wfname)) {
					trace("file %s found\n", fname);
					found_file = true;
				}
			}
			break;

		default:
			break;
		}

		if (found_file)
			break;
		else
			arc->SeekToNext();
	}
	if (!found_file)
		return NULL;

	// Seek to head of the file
	arc->Seek(arc->NextBlockPos - arc->FileHead.PackSize, SEEK_SET);

	// Initialize the ComprDataIO and Unpack
	ComprDataIO *dataIO = new ComprDataIO();
	Unpack *unp = new Unpack(dataIO);
	SecPassword secpwd;

	byte pswcheck[SIZE_PSWCHECK];
	dataIO->SetEncryption(
		false,
		arc->FileHead.CryptMethod,
		&secpwd,
		arc->FileHead.SaltSet ? arc->FileHead.Salt : NULL,
		arc->FileHead.InitV,
		arc->FileHead.Lg2Count,
		pswcheck,
		arc->FileHead.HashKey
	);

	dataIO->CurUnpRead = 0;
	dataIO->CurUnpWrite = 0;
	dataIO->UnpHash.Init(arc->FileHead.FileHash.Type, 1);
	dataIO->PackedDataHash.Init(arc->FileHead.FileHash.Type, 1);
	dataIO->SetPackedSizeToRead(arc->FileHead.PackSize);
	dataIO->SetTestMode(true);
	dataIO->SetSkipUnpCRC(true);

	dataIO->SetFiles(arc, NULL); // we unpack data to memory

	// Unpack the full file into memory
	byte *buffer = (byte *)malloc(arc->FileHead.UnpSize);
	dataIO->SetUnpackToMemory(buffer, arc->FileHead.UnpSize);

	if (arc->FileHead.Method == 0)
		unstore_file(dataIO, arc->FileHead.UnpSize);
	else {
		unp->Init(arc->FileHead.WinSize, arc->FileHead.Solid);
		unp->SetDestSize(arc->FileHead.UnpSize);
		if (arc->Format != RARFMT50 && arc->FileHead.UnpVer <= 15)
			unp->DoUnpack(15, arc->Solid);
		else
			unp->DoUnpack(arc->FileHead.UnpVer, arc->FileHead.Solid);
	}

	rar_file_t *rf = (rar_file_t *)malloc (sizeof (rar_file_t));
	memset (rf, 0, sizeof (rar_file_t));
	rf->file.vfs = &plugin;
	rf->arc = arc;
	rf->dataIO = dataIO;
	rf->unp = unp;
	rf->buffer = buffer;
	rf->offset = 0;
	rf->size = arc->FileHead.UnpSize;

	return (DB_FILE*)rf;
}

void
vfs_rar_close (DB_FILE *f)
{
	rar_file_t *rf = (rar_file_t *)f;

	if(rf->buffer)
		free(rf->buffer);
	if (rf->unp)
		delete rf->unp;
	if (rf->dataIO)
		delete rf->dataIO;
	if (rf->arc)
		delete rf->arc;

	free (rf);
}

size_t
vfs_rar_read (void *ptr, size_t size, size_t nmemb, DB_FILE *f)
{
	trace("[vfs_rar_read]\n");
	rar_file_t *rf = (rar_file_t *)f;

	size_t rb = min((int64_t)(size * nmemb), (rf->size - rf->offset));
	memcpy(ptr, rf->buffer + rf->offset, rb);
	rf->offset += rb;

	return (rb/size);
}

int
vfs_rar_seek (DB_FILE *f, int64_t offset, int whence)
{
	trace("[vfs_rar_seek]");
	rar_file_t *rf = (rar_file_t *)f;

	if (whence == SEEK_CUR) {
		offset = rf->offset + offset;
	}
	else if (whence == SEEK_END) {
		offset = rf->size + offset;
	}

	if (offset < 0 || offset > rf->size)
		return -1;

	rf->offset = offset;
	return 0;
#if 0
	// reopen when seeking back
	if (offset < rf->offset) {
		rf->arc->Seek(
			rf->arc->NextBlockPos - rf->arc->NewLhd.FullPackSize,
			SEEK_SET
		);
		rf->offset = 0;
	}

	unsigned char buf[4096];
	int64_t n = offset - rf->offset;
	while (n > 0) {
		int sz = min (n, sizeof (buf));
		size_t rb = read_unpacked_data(rf, buf, sz);
		n -= rb;
		assert (n >= 0);
		rf->offset += rb;
		if (rb != sz) {
			break;
		}
	}

	return n > 0 ? -1 : 0;
#endif
}

int64_t
vfs_rar_tell (DB_FILE *f)
{
	rar_file_t *rf = (rar_file_t *)f;
	return rf->offset;
}

void
vfs_rar_rewind (DB_FILE *f)
{
	rar_file_t *rf = (rar_file_t *)f;
	rf->offset = 0;
}

int64_t
vfs_rar_getlength (DB_FILE *f)
{
	rar_file_t *rf = (rar_file_t *)f;
	return rf->size;
}

int
vfs_rar_scandir (
	const char *dir,
	struct dirent ***namelist,
	int (*selector) (const struct dirent *),
	int (*cmp) (const struct dirent **, const struct dirent **)
)
{
	vector<string> fname_list;
	Archive arc;

	wchar_t wdir[NM];
	UtfToWide(dir, wdir, ASIZE(wdir)-1);

	if (!arc.WOpen(wdir))
		return -1;

	if (!arc.IsArchive(true))
		return -1;

	// read files from archive
	while (arc.ReadHeader() > 0) {
		int hdr_type = arc.GetHeaderType();
		if (hdr_type == HEAD_ENDARC)
			break;
	
		switch (hdr_type) {
		case HEAD_FILE:
			if (!arc.IsArcDir()) {
				char fname[wcslen(arc.FileHead.FileName)*2];
				WideToUtf(arc.FileHead.FileName, fname, ASIZE(fname));
				fname_list.push_back(string(fname));
			}
			break;

		default:
			break;
		}

		arc.SeekToNext();
	}

	// transmit files to player
	int n = fname_list.size();
	*namelist = (dirent **)malloc (sizeof(void *) * n);
	for (int i = 0; i < n; i++) {
		(*namelist)[i] = (dirent *)malloc (sizeof(struct dirent));
		memset ((*namelist)[i], 0, sizeof(struct dirent));
		//snprintf(
		snprintf(
			(*namelist)[i]->d_name, sizeof((*namelist)[i]->d_name),
			"rar://%s:%s", dir, fname_list[i].c_str()
		);
	}

	return n;
}

int
vfs_rar_is_container (const char *fname)
{
	const char *ext = strrchr (fname, '.');
	if (ext && !strcasecmp (ext, ".rar"))
		return 1;
	return 0;
}

extern "C"
DB_plugin_t *
vfs_rar_load (DB_functions_t *api)
{
	deadbeef = api;

	plugin.plugin.api_vmajor = 1;
	plugin.plugin.api_vminor = 0;
	plugin.plugin.version_major = 1;
	plugin.plugin.version_minor = 9;
	plugin.plugin.type = DB_PLUGIN_VFS;
	plugin.plugin.id = "vfs_rar";
	plugin.plugin.name = "RAR vfs";
	plugin.plugin.descr = "play files directly from rar files";
	plugin.plugin.copyright =
		"MIT License\n"
		"\n"
		"Copyright (c) 2017 Shao Hao\n"
		"\n"
		"Permission is hereby granted, free of charge, to any person obtaining a copy\n"
		"of this software and associated documentation files (the \"Software\"), to deal\n"
		"in the Software without restriction, including without limitation the rights\n"
		"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
		"copies of the Software, and to permit persons to whom the Software is\n"
		"furnished to do so, subject to the following conditions:\n"
		"\n"
		"The above copyright notice and this permission notice shall be included in all\n"
		"copies or substantial portions of the Software.\n"
		"\n"
		"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
		"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
		"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
		"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
		"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
		"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
		"SOFTWARE.\n"
		"\n"
		"\n"
		"UnRAR source (C) Alexander RoshalUnRAR";
	plugin.plugin.website = "http://github.com/shaohao/vfs_rar";
	plugin.open = vfs_rar_open;
	plugin.close = vfs_rar_close;
	plugin.read = vfs_rar_read;
	plugin.seek = vfs_rar_seek;
	plugin.tell = vfs_rar_tell;
	plugin.rewind = vfs_rar_rewind;
	plugin.getlength = vfs_rar_getlength;
	plugin.get_schemes = vfs_rar_get_schemes;
	plugin.is_streaming = vfs_rar_is_streaming;
	plugin.is_container = vfs_rar_is_container;
	plugin.scandir = vfs_rar_scandir;

	return DB_PLUGIN(&plugin);
}

