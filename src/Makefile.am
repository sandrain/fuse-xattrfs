AM_CFLAGS = -Wall -Werror $(FUSE_CFLAGS) $(SQLITE3_CFLAGS)

bin_PROGRAMS = xattrfs

xattrfs_SOURCES = xattrfs.c xattrfs.h \
		  xattrfs-fops.c        \
		  xattrfs-xdb.c       \
		  xattrfs-schema.c

xattrfs_LDADD = $(FUSE_LIBS)
xattrfs_LDADD += $(SQLITE3_LIBS)

xattrfs-schema.c: xattrfs-schema.sql
	@( echo "const char xdb_schema_sqlstr[] = ";\
	   sed 's/^/"/; s/$$/\\n"/' < $< ;\
	   echo ";" ) > $@

CLEANFILES = xattrfs-schema.c

