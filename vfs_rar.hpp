#ifndef VFS_RAR_HPP
#define VFS_RAR_HPP

#include <deadbeef/deadbeef.h>

const char ** vfs_rar_get_schemes (void);

int vfs_rar_is_streaming (void);

DB_FILE* vfs_rar_open (const char *fname);
void vfs_rar_close (DB_FILE *f);

size_t vfs_rar_read (void *ptr, size_t size, size_t nmemb, DB_FILE *f);

int vfs_rar_seek (DB_FILE *f, int64_t offset, int whence);

int64_t vfs_rar_tell (DB_FILE *f);

void vfs_rar_rewind (DB_FILE *f);

int64_t vfs_rar_getlength (DB_FILE *f);

int vfs_rar_scandir (
	const char *dir,
	struct dirent ***namelist,
	int (*selector) (const struct dirent *),
	int (*cmp) (const struct dirent **, const struct dirent **)
);

int vfs_rar_is_container (const char *fname);

#endif // VFS_RAR_HPP

