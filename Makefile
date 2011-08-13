CPPFLAGS = -c -fPIC \
	-I./ -I./unrar \
	-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE
LDFLAGS = -shared -fPIC
DST_OBJS = vfs_rar.o
RARLIB_OBJS =  $(addprefix unrar/, \
	strlist.o \
	strfn.o \
	pathfn.o \
	savepos.o \
	smallfn.o \
	global.o \
	file.o \
	filefn.o \
	filcreat.o \
	archive.o \
	arcread.o \
	unicode.o \
	system.o \
	isnt.o \
	crypt.o \
	crc.o \
	rawread.o \
	encname.o \
	resource.o \
	match.o \
	timefn.o \
	rdwrfn.o \
	consio.o \
	options.o \
	ulinks.o \
	errhnd.o \
	rarvm.o \
	rijndael.o \
	getbits.o \
	sha1.o \
	extinfo.o \
	extract.o \
	volume.o \
	list.o \
	find.o \
	unpack.o \
	cmddata.o \
	recvol.o scantree.o rs.o filestr.o \
)

vfs_rar.so: $(RARLIB_OBJS) $(DST_OBJS)
	g++ $(LDFLAGS) $^ -o $@

install:
	-mkdir -p ~/.local/lib/deadbeef
	-cp vfs_rar.so ~/.local/lib/deadbeef

uninstall:
	-rm -rf ~/.local/lib/deadbeef/vfs_rar.so

clean:
	-rm -rf *.so *.o unrar/*.o

