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
#include <sqlite3.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>		/* XATTR_CREATE, XATTR_REPLACE */
#include <attr/xattr.h>		/* ENOATTR */
#include <unistd.h>

#include "xattrfs.h"

extern const char xdb_schema_sqlstr[];

enum {
	XATTR_NS_NONE		= 0,
	XATTR_NS_SYSTEM,
	XATTR_NS_SECURITY,
	XATTR_NS_TRUSTED,
	XATTR_NS_USER,

	N_XATTR_NS
};

static const char *nsstr[] = {
	NULL, "system.", "security.", "trusted.", "user."
};

static const uint32_t nslens[] = {	/* including the dot */
	0,
	7,	/* [1] system. */
	9,	/* [2] security. */
	8,	/* [3] trusted. */
	5,	/* [4] user. */
};

static inline const char *get_nsstr(const int ns)
{
	return nsstr[ns];
}

static inline uint64_t get_nsstrlen(const int ns)
{
	return nslens[ns];
}

enum {
	INSERT_NEW_XATTR = 0,
	UPDATE_XATTR,
	SEARCH_XATTR,
	SEARCH_LEN_XATTR,
	REMOVE_XATTR,
	LIST_XATTR,

	N_XDB_SQLS
};

static const char *xdb_sqls[N_XDB_SQLS] = {
/* [INSERT_NEW_XATTR] */
	"INSERT INTO xdb_xattr (ino, nid, name, value) VALUES (?,?,?,?)",
/* [UPDATE_XATTR] */
	"UPDATE xdb_xattr SET value=? WHERE ino=? AND nid=? AND name=?",
/* [SEARCH_XATTR] */
	"SELECT value FROM xdb_xattr WHERE ino=? AND nid=? AND name=?",
/* [SEARCH_LEN_XATTR] */
	"SELECT length(value) FROM xdb_xattr WHERE ino=? AND nid=? AND name=?",
/* [REMOVE_XATTR] */
	"DELETE FROM xdb_xattr WHERE ino=? AND nid=? AND name=?",
/* [LIST_XATTR] */
	"SELECT nid,name FROM xdb_xattr WHERE ino=?",
};

static inline int exec_simple_sql(struct xdb *self, const char *sql)
{
	int ret = sqlite3_exec(self->conn, sql, NULL, NULL, NULL);
	return ret == SQLITE_OK ? 0 : ret;
}

static inline int tx_begin(struct xdb *self)
{
	return exec_simple_sql(self, "BEGIN TRANSACTION");
}

static inline int tx_end(struct xdb *self)
{
	return exec_simple_sql(self, "END TRANSACTION");
}

static inline int tx_abort(struct xdb *self)
{
	return exec_simple_sql(self, "ROLLBACK");
}

static inline int str_startswith(const char *str, char *pre)
{
	return 0 == strncmp(str, pre, strlen(pre));
}

static inline char *get_db_path(const char *dir)
{
	char buf[1024];

	sprintf(buf, "%s/%s", dir, XDB_FILE);

	return strdup(buf);
}

/** returns 0 if the namespace is not valid.
 */
static inline int get_ns(const char *name)
{
	if (str_startswith(name, "system."))
		return XATTR_NS_SYSTEM;
	else if (str_startswith(name, "security."))
		return XATTR_NS_SECURITY;
	else if (str_startswith(name, "trusted."))
		return XATTR_NS_TRUSTED;
	else if (str_startswith(name, "user."))
		return XATTR_NS_USER;
	else
		return XATTR_NS_NONE;
}

static inline int get_ino (const char *path, ino_t *ino)
{
	int ret;
	struct stat st;

	ret = stat (path, &st);
	if (ret == -1)
		return ret;

	*ino = st.st_ino;

	return 0;
}

static inline char *attr_name (const char *name)
{
	char *pos = strchr(name, '.');
	return pos ? &pos[1] : (char *) name;
}


static int db_initialize(struct xdb *self)
{
	int ret = 0;
	int ntables = 0;
	sqlite3_stmt *stmt;
	char *sql = "select count(type) from sqlite_master where type='table'"
		    "and name='xdb_xattr' or name='xdb_ns';";

	ret = sqlite3_prepare_v2(self->conn, sql, -1, &stmt, 0);
	if (ret != SQLITE_OK) {
		ret = -EIO;
		goto out;
	}

	ret = sqlite3_step(stmt);
	if (ret != SQLITE_ROW) {
		ret = -EIO;
		goto out;
	}

	ntables = sqlite3_column_int(stmt, 0);
	if (ntables == 0) {
		ret = exec_simple_sql(self, xdb_schema_sqlstr);
		if (ret) {
			ret = -EIO;
			goto out;
		}
	}

	sqlite3_finalize(stmt);
out:
	return ret;
}

static ssize_t
do_real_getxattr(struct xdb *self, const ino_t ino, const char *name,
			char *value, size_t size)
{
	ssize_t ret = 0;
	const void *val = NULL;
	int ns = get_ns(name);
	sqlite3_stmt *stmt = NULL;
	
	ret = sqlite3_prepare_v2(self->conn, xdb_sqls[SEARCH_XATTR],
				 -1, &stmt, NULL);
	if (ret != SQLITE_OK)
		return -EIO;

	ret = sqlite3_bind_int64(stmt, 1, ino);
	ret |= sqlite3_bind_int(stmt, 2, ns);
	ret |= sqlite3_bind_text(stmt, 3, attr_name(name), -1, SQLITE_STATIC);
	if (ret) {
		ret = -EIO;
		goto out;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	if (ret != SQLITE_ROW) {
		ret = ret == SQLITE_DONE ? -ENODATA : -EIO;
		goto out;
	}

	ret = sqlite3_column_bytes(stmt, 0);
	val = sqlite3_column_blob(stmt, 0);

	if (size < ret) {
		ret = -ERANGE;
		goto out;
	}

	memcpy(value, val, ret);
out:
	sqlite3_finalize(stmt);
	return ret;
}


static ssize_t
do_len_getxattr(struct xdb *self, const ino_t ino, const char *name)
{
	ssize_t ret = 0;
	int ns = get_ns(name);
	sqlite3_stmt *stmt = NULL;
	
	ret = sqlite3_prepare_v2(self->conn, xdb_sqls[SEARCH_LEN_XATTR],
				 -1, &stmt, NULL);
	if (ret != SQLITE_OK)
		return -EIO;

	ret = sqlite3_bind_int64(stmt, 1, ino);
	ret |= sqlite3_bind_int(stmt, 2, ns);
	ret |= sqlite3_bind_text(stmt, 3, attr_name(name), -1, SQLITE_STATIC);
	if (ret) {
		ret = -EIO;
		goto out;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	if (ret != SQLITE_ROW) {
		ret = ret == SQLITE_DONE ? -ENODATA : -EIO;
		goto out;
	}

	ret = sqlite3_column_int(stmt, 0);

out:
	sqlite3_finalize(stmt);
	return ret;
}

static
int do_update_xattr(struct xdb *self, ino_t ino, const char *name,
			const void *value, size_t size)
{
	int ret = 0;
	int ns = get_ns(name);
	sqlite3_stmt *stmt = NULL;

	ret = sqlite3_prepare_v2(self->conn, xdb_sqls[UPDATE_XATTR],
				 -1, &stmt, NULL);
	if (ret != SQLITE_OK)
		return -EIO;

	ret = sqlite3_bind_blob(stmt, 1, value, (int) size, NULL);
	ret |= sqlite3_bind_int64(stmt, 2, ino);
	ret |= sqlite3_bind_int(stmt, 3, ns);
	ret |= sqlite3_bind_text(stmt, 4, attr_name(name), -1, SQLITE_STATIC);
	if (ret) {
		ret = -EIO;
		goto out;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	ret = ret == SQLITE_DONE ? 0 : -EIO;

out:
	sqlite3_finalize(stmt);
	return ret;
}

static
int do_create_xattr(struct xdb *self, ino_t ino, const char *name,
			const void *value, size_t size)
{
	int ret = 0;
	int ns = get_ns(name);
	sqlite3_stmt *stmt = NULL;

	ret = sqlite3_prepare_v2(self->conn, xdb_sqls[INSERT_NEW_XATTR],
				 -1, &stmt, NULL);
	if (ret != SQLITE_OK)
		return -EIO;

	ret = sqlite3_bind_int64(stmt, 1, ino);
	ret |= sqlite3_bind_int(stmt, 2, ns);
	ret |= sqlite3_bind_text(stmt, 3, attr_name(name), -1, SQLITE_STATIC);
	ret |= sqlite3_bind_blob(stmt, 4, value, (int) size, NULL);
	if (ret) {
		ret = -EIO;
		goto out;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	ret = ret == SQLITE_DONE ? 0 : -EIO;

out:
	sqlite3_finalize(stmt);
	return ret;
}

/**
 * external interface
 */

int xdb_init(struct xdb **xdb, const char *dir)
{
	int ret = 0;
	char *dbpath = NULL;
	sqlite3 *conn = NULL;
	struct xdb *self = NULL;

	self = calloc(1, sizeof(*self) + N_XDB_SQLS * sizeof(sqlite3_stmt *));
	if (!self)
		return -1;

	dbpath = get_db_path(dir);
	if (!dbpath)
		goto out;

	ret = sqlite3_open(dbpath, &conn);
	if (ret != SQLITE_OK) {
		ret = -EIO;
		goto out;
	}

	self->conn = conn;
	self->dbpath = dbpath;

	ret = db_initialize(self);
	if (ret)
		goto out;

	*xdb = self;

	return 0;

out:
	free(self);
	return ret;
}

void xdb_exit(struct xdb *xdb)
{
	if (xdb) {
		if (xdb->dbpath)
			free((void *) xdb->dbpath);

		if (xdb->conn)
			sqlite3_close(xdb->conn);
		free(xdb);
	}
}

int xdb_getxattr(struct xdb *xdb, ino_t ino,
			const char *name, char *value, size_t size)
{
	return size ? do_real_getxattr(xdb, ino, name, value, size)
		    : do_len_getxattr(xdb, ino, name);
}

int xdb_setxattr(struct xdb *xdb, ino_t ino, const char *name,
			const char *value, size_t size, int flags)
{
	int ret = 0;
	ssize_t len = 0;

	/* search if the given named attr is already set. */
	len = do_len_getxattr(xdb, ino, name);

	if (len == -EIO) {
		ret = -EIO;
		goto out;
	}
	else if (len == -ENODATA) {
		if (flags & XATTR_REPLACE) {
			ret = -ENOATTR;
			goto out;
		}

		len = 0;	/* set a new xattr */
	}
	else {
		if (flags & XATTR_CREATE) {
			ret = -EEXIST;
			goto out;
		}
	}

	return len ? do_update_xattr(xdb, ino, name, value, size)
		   : do_create_xattr(xdb, ino, name, value, size);

out:
	return ret;
}

int xdb_removexattr(struct xdb *xdb, ino_t ino, const char *name)
{
	int ret = 0;
	int ns = get_ns(name);
	sqlite3_stmt *stmt = NULL;

	ret = sqlite3_prepare_v2(xdb->conn, xdb_sqls[REMOVE_XATTR],
				 -1, &stmt, NULL);
	if (ret != SQLITE_OK)
		return -EIO;

	ret = sqlite3_bind_int64(stmt, 1, ino);
	ret |= sqlite3_bind_int(stmt, 2, ns);
	ret |= sqlite3_bind_text(stmt, 3, attr_name(name), -1, SQLITE_STATIC);
	if (ret) {
		ret = -EIO;
		goto out;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	if (ret != SQLITE_DONE) {
		ret = -EIO;
		goto out;
	}

	if (sqlite3_changes(xdb->conn) == 0) {
		ret = -ENOENT;
		goto out;
	}
	ret = 0;

out:
	sqlite3_finalize(stmt);
	return ret;
}

int xdb_listxattr(struct xdb *xdb, ino_t ino, char *list, size_t size)
{
	int ret = 0;
	ssize_t bytes = 0;
	sqlite3_stmt *stmt = NULL;
	char *pos = list;
	int mode_fill = (list != NULL && size > 0);

	ret = sqlite3_prepare_v2(xdb->conn, xdb_sqls[LIST_XATTR],
				 -1, &stmt, NULL);
	if (ret != SQLITE_OK)
		return -EIO;

	ret = sqlite3_bind_int64(stmt, 1, ino);
	if (ret) {
		ret = -EIO;
		goto out;
	}

	while (SQLITE_ROW == (ret = sqlite3_step(stmt))) {
		int ns = sqlite3_column_int(stmt, 0);
		const char *name = (char *) sqlite3_column_text(stmt, 1);

		/** FIXME!! handle exceptions here accordingly */

		if (mode_fill) {
			bytes += sprintf(pos, "%s%s%c",
					get_nsstr(ns), name, '\0');
			pos = &list[bytes];
		}
		else
			bytes += get_nsstrlen(ns) + strlen(name) + 1;
	}

	if (ret != SQLITE_DONE) {
		errno = -EIO;
		goto out;
	}

	ret = bytes;
out:
	sqlite3_finalize(stmt);
	return ret;
}

