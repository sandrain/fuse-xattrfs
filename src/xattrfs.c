/* Copyright (C) 2015	 - Hyogi Sim <simh@ornl.gov>
 * 
 * Please refer to COPYING for the license.
 * ---------------------------------------------------------------------------
 * 
 */
#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>

#define FUSE_USE_VERSION		26
#include <fuse.h>

#include "xattrfs.h"

#ifdef DEBUG
static int debug = 1;
#else
static int debug;
#endif

static char *fsroot;
static char *mntpnt;

static void usage(void)
{
	printf("Usage: %s [OPTIONS].. b:<basedir> <mountpoint>\n\n"
	       "options:\n"
	       "  -d, --debug           Enable debug mode\n"
	       "  -h, --help            This help message\n\n",
	       PACKAGE_NAME);
}

enum { OPTKEY_DEBUG = 0, OPTKEY_HELP, OPTKEY_BASEDIR };

static struct fuse_opt xattrfs_opts[] = {
	FUSE_OPT_KEY("-d", OPTKEY_DEBUG),
	FUSE_OPT_KEY("--debug", OPTKEY_DEBUG),
	FUSE_OPT_KEY("-h", OPTKEY_HELP),
	FUSE_OPT_KEY("--help", OPTKEY_HELP),
	FUSE_OPT_END
};

/* get absolute pathname, with '/' appended */
static char *get_real_path(const char *path)
{
	char *rpath = realpath(path, NULL);

	if (rpath) {
		char *last = &rpath[strlen(rpath)-1];
		if (*last != '/') {
			last[1] = '/';
			last[2] = '\0';
		}
	}

	return rpath;
}

/* should return -1 on error,
 * 0 if arg is to be discarded, 1 if arg should be kept
 */
static int xattrfs_process_opt(void *data, const char *arg, int key,
				struct fuse_args *outargs)
{
	switch (key) {
	case OPTKEY_DEBUG:
		debug = 1;
		return 1;
	case OPTKEY_HELP:
		usage();
		exit(1);
	default:
		if (arg[0] == 'b' && arg[1] == ':') {
			fsroot = get_real_path(&arg[2]);	/* basedir */
			return 0;
		}
		else if (arg[0] != '-') {
			mntpnt = get_real_path(arg);		/* mountpoint */
			return 1;
		}
		else
			return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct xattrfs_ctx *ctx;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	if (fuse_opt_parse(&args, NULL, xattrfs_opts, xattrfs_process_opt) < 0)
		return EINVAL;

#ifdef FUSE_CAP_BIG_WRITES
	if (fuse_opt_add_arg(&args, "-obig_writes")) {
		fputs("failed to enable big writes (-obig_writes).\n", stderr);
		exit(1);
	}
#endif
	if (fuse_opt_add_arg(&args, "-ouse_ino")) {
		fputs("failed to set an inode option (-ouse_ino).\n", stderr);
		exit(1);
	}

	if (!fsroot) {
		fputs("base directory was not given, exiting..\n", stderr);
		return EINVAL;
	}

	if (!mntpnt) {
		fputs("mountpoint was not given, exiting..\n", stderr);
		return EINVAL;
	}

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		perror("calloc");
		return ENOMEM;
	}

	ctx->mntpnt = mntpnt;
	ctx->fsroot = fsroot;
	ctx->debug = debug;

	return fuse_main(args.argc, args.argv, &xattrfs_fops, ctx);
}

