#ifndef _XATTRFS_H_
#define _XATTRFS_H_

#include <config.h>
#include <fuse.h>
#include <sqlite3.h>

/**
 * xdb interface, implemented at xattrfs-xdb.c
 */

#define XDB_FILE		".xattr.db"

struct xdb {
	const char *dbpath;
	sqlite3 *conn;
};

int xdb_init(struct xdb **xdb, const char *dir);

void xdb_exit(struct xdb *xdb);

/**
 * all functions are working with inode number (stat.st_ino).
 */

int xdb_getxattr(struct xdb *xdb, ino_t ino,
			const char *name, char *value, size_t size);

int xdb_setxattr(struct xdb *xdb, ino_t ino, const char *name,
			const char *value, size_t size, int flags);

int xdb_removexattr(struct xdb *xdb, ino_t ino, const char *name);

int xdb_listxattr(struct xdb *xdb, ino_t ino, char *list, size_t size);

/**
 * fuse implementation at xattrfs-fuse.c
 */

struct xattrfs_ctx {
	int debug;
	const char *mntpnt;
	const char *fsroot;
	struct xdb *xdb;
};

extern struct fuse_operations xattrfs_fops;

#define _llu(x)			((unsigned long long) (x))

#endif /* _XATTRFS_H_ */

