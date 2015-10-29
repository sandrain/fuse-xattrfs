/* Copyright (C) 2015	 - Hyogi Sim <simh@ornl.gov>
 * 
 * Please refer to COPYING for the license.
 * ---------------------------------------------------------------------------
 * basic implementations mostly taken from bbfs (big brother file system):
 * http://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/
 */
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#define FUSE_USE_VERSION		26
#include <fuse.h>

#include "xattrfs.h"

#define get_xattrfs_ctx	\
		((struct xattrfs_ctx *) (fuse_get_context()->private_data))

static inline
char *fullpath(struct xattrfs_ctx *ctx, const char *path, char *buf)
{
	sprintf(buf, "%s%s", ctx->fsroot, path);
	return buf;
}

/**
 * FUSE operations: every function should return negated errno (-errno) instead
 * of -1, on error.
 */

static int xattrfs_getattr(const char *path, struct stat *stbuf)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];

	ret = lstat(fullpath(ctx, path, buf), stbuf);

	return ret < 0 ? -errno : ret;
}

/* the link should be null-terminated. on success, should return 0 */
static int xattrfs_readlink(const char *path, char *link, size_t size)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];

	ret = readlink(fullpath(ctx, path, buf), link, size);

	return ret < 0 ? -errno : 0;
}

/* FIXME: creation of all non-directory, non-symlink nodes */
static int xattrfs_mknod(const char *path, mode_t mode, dev_t dev)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];

	ret = mknod(fullpath(ctx, path, buf), mode, dev);

	return ret < 0 ? -errno : ret;
}

static int xattrfs_mkdir(const char *path, mode_t mode)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];

	ret = mkdir(fullpath(ctx, path, buf), mode);

	return ret < 0 ? -errno : ret;
}

static int xattrfs_unlink(const char *path)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];

	ret = unlink(fullpath(ctx, path, buf));

	return ret < 0 ? -errno : ret;
}

static int xattrfs_rmdir(const char *path)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];

	ret = rmdir(fullpath(ctx, path, buf));

	return ret < 0 ? -errno : ret;
}

static int xattrfs_symlink(const char *path, const char *link)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];

	ret = symlink(path, fullpath(ctx, link, buf));

	return ret < 0 ? -errno : ret;
}

static int xattrfs_rename(const char *old, const char *new)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char oldpath[PATH_MAX];
	char newpath[PATH_MAX];

	ret = rename(fullpath(ctx, old, oldpath),
		     fullpath(ctx, new, newpath));

	return ret < 0 ? -errno : ret;
}

static int xattrfs_link(const char *path, const char *new)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char oldpath[PATH_MAX];
	char newpath[PATH_MAX];

	ret = link(fullpath(ctx, path, oldpath),
		   fullpath(ctx, new, newpath));

	return ret < 0 ? -errno : ret;
}

static int xattrfs_chmod(const char *path, mode_t mode)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];

	ret = chmod(fullpath(ctx, path, buf), mode);

	return ret < 0 ? -errno : ret;
}

static int xattrfs_chown(const char *path, uid_t uid, gid_t gid)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];

	ret = chown(fullpath(ctx, path, buf), uid, gid);

	return ret < 0 ? -errno : ret;
}

static int xattrfs_truncate(const char *path, off_t newsize)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];

	ret = truncate(fullpath(ctx, path, buf), newsize);

	return ret < 0 ? -errno : ret;
}

static int xattrfs_utime(const char *path, struct utimbuf *tbuf)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];

	ret = utime(fullpath(ctx, path, buf), tbuf);

	return ret < 0 ? -errno : ret;
}

static int xattrfs_open(const char *path, struct fuse_file_info *fi)
{
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];
	int fd;

	fd = open(fullpath(ctx, path, buf), fi->flags);
	if (fd > 0) {
		fi->fh = fd;
		return 0;
	}
	else
		return -errno;
}

static int xattrfs_read(const char *path, char *buf, size_t size,
			off_t offset, struct fuse_file_info *fi)
{
	return pread(fi->fh, buf, size, offset);
}

static int xattrfs_write(const char *path, const char *buf, size_t size,
			off_t offset, struct fuse_file_info *fi)
{
	return pwrite(fi->fh, buf, size, offset);
}

static int xattrfs_statfs(const char *path, struct statvfs *vfs)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];

	ret = statvfs(fullpath(ctx, path, buf), vfs);

	return ret < 0 ? -errno : ret;
}

static int xattrfs_flush(const char *path, struct fuse_file_info *fi)
{
	/** this is called upon each close() call */
	return 0;
}

static int xattrfs_release(const char *path, struct fuse_file_info *fi)
{
	/** no more references to the file handle. */
	return close(fi->fh);
}

static int xattrfs_fsync(const char *path, int datasync,
			struct fuse_file_info *fi)
{
	int ret = 0;

#ifdef HAVE_FDATASYNC
	if (datasync)
		ret = fdatasync(fi->fh);
	else
#endif
		ret = fsync(fi->fh);

	return ret < 0 ? -errno : ret;
}

/**
 * supporting extended attributes is optional.
 */

static int xattrfs_setxattr(const char *path, const char *name,
			const char *value, size_t size, int flags)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];
	struct stat sb;

	ret = stat(fullpath(ctx, path, buf), &sb);
	if (ret)
		return -errno;

	return xdb_setxattr(ctx->xdb, sb.st_ino, name, value, size, flags);
}

static int xattrfs_getxattr(const char *path, const char *name,
			char *value, size_t size)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];
	struct stat sb;

	ret = stat(fullpath(ctx, path, buf), &sb);
	if (ret)
		return -errno;

	return xdb_getxattr(ctx->xdb, sb.st_ino, name, value, size);
}

static int xattrfs_listxattr(const char *path, char *list, size_t size)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];
	struct stat sb;

	ret = stat(fullpath(ctx, path, buf), &sb);
	if (ret)
		return -errno;

	return xdb_listxattr(ctx->xdb, sb.st_ino, list, size);
}

static int xattrfs_removexattr(const char *path, const char *name)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];
	struct stat sb;

	ret = stat(fullpath(ctx, path, buf), &sb);
	if (ret)
		return -errno;

	return xdb_removexattr(ctx->xdb, sb.st_ino, name);
}

/**
 * our implementation of readdir is stateless.
 */

static int xattrfs_opendir(const char *path, struct fuse_file_info *fi)
{
	DIR *dp;
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];

	dp = opendir(fullpath(ctx, path, buf));
	if (!dp)
		return -errno;

	fi->fh = (intptr_t) dp;

	return 0;
}

static int xattrfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			off_t offset, struct fuse_file_info *fi)
{
	struct dirent *de;
	DIR *dp = (DIR *) (uintptr_t) fi->fh;

	de = readdir(dp);
	if (!de)
		return -errno;

	do {
		if (filler(buf, de->d_name, NULL, 0))
			return -ENOMEM;
	} while ((de = readdir(dp)) != NULL);

	return 0;
}

static int xattrfs_releasedir(const char *path, struct fuse_file_info *fi)
{
	closedir((DIR *) (uintptr_t) fi->fh);
	return 0;
}

static int xattrfs_fsyncdir(const char *path, int datasync,
				struct fuse_file_info *fi)
{
	return 0;
}

/**
 * Undocumented but extraordinarily useful fact:  the fuse_context is set up
 * before this function is called, and fuse_get_context()->private_data returns
 * the user_data passed to fuse_main().  Really seems like either it should be
 * a third parameter coming in here, or else the fact should be documented (and
 * this might as well return void, as it did in older versions of FUSE).
 */
static void *xattrfs_init(struct fuse_conn_info *conn)
{
	int ret = 0;
	struct xattrfs_ctx *ctx = (struct xattrfs_ctx *)
					fuse_get_context()->private_data;
	struct xdb *xdb;

	ret = xdb_init(&xdb, ctx->fsroot);
	if (ret)
		return NULL;

	ctx->xdb = xdb;

	return ctx;
}

static void xattrfs_destroy(void *context)
{
	struct xattrfs_ctx *ctx = (struct xattrfs_ctx *) context;

	if (ctx->xdb)
		xdb_exit(ctx->xdb);
	if (ctx->fsroot)
		free((void *) ctx->fsroot);
}

static int xattrfs_access(const char *path, int mask)
{
	struct xattrfs_ctx *ctx = get_xattrfs_ctx;
	char buf[PATH_MAX];

	return access(fullpath(ctx, path, buf), mask);
}

static int xattrfs_ftruncate(const char *path, off_t newsize,
				struct fuse_file_info *fi)
{
	return ftruncate(fi->fh, newsize);
}

static int xattrfs_fgetattr(const char *path, struct stat *stbuf,
			struct fuse_file_info *fi)
{
	return fstat(fi->fh, stbuf);
}

static int xattrfs_utimens(const char *path, const struct timespec tv[2])
{
	return -ENOSYS;
}


struct fuse_operations xattrfs_fops = {
	.getattr	= xattrfs_getattr,
	.readlink	= xattrfs_readlink,
	.getdir		= NULL,
	.mknod		= xattrfs_mknod,
	.mkdir		= xattrfs_mkdir,
	.unlink		= xattrfs_unlink,
	.rmdir		= xattrfs_rmdir,
	.symlink	= xattrfs_symlink,
	.rename		= xattrfs_rename,
	.link		= xattrfs_link,
	.chmod		= xattrfs_chmod,
	.chown		= xattrfs_chown,
	.truncate	= xattrfs_truncate,
	.utime		= xattrfs_utime,
	.open		= xattrfs_open,
	.read		= xattrfs_read,
	.write		= xattrfs_write,
	.statfs		= xattrfs_statfs,
	.flush		= xattrfs_flush,
	.release	= xattrfs_release,
	.fsync		= xattrfs_fsync,
	.setxattr	= xattrfs_setxattr,
	.getxattr	= xattrfs_getxattr,
	.listxattr	= xattrfs_listxattr,
	.removexattr	= xattrfs_removexattr,
	.opendir	= xattrfs_opendir,
	.readdir	= xattrfs_readdir,
	.releasedir	= xattrfs_releasedir,
	.fsyncdir	= xattrfs_fsyncdir,
	.init		= xattrfs_init,
	.destroy	= xattrfs_destroy,
	.access		= xattrfs_access,
	.create		= NULL,
	.ftruncate	= xattrfs_ftruncate,
	.fgetattr	= xattrfs_fgetattr,
	.lock		= NULL,
	/** currently, stick to deprecated utime(2) */
	.utimens	= xattrfs_utimens,
	.bmap		= NULL,
};

