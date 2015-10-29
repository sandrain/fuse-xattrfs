#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([xattrfs], [0.1], [hyogi@vt.edu])
AC_CONFIG_SRCDIR([src/xattrfs.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall -Werror])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

# Checks for libraries.
PKG_CHECK_MODULES([FUSE], [fuse], ,
	AC_MSG_ERROR(['FUSE is required to build xattrfs.']))
PKG_CHECK_MODULES([SQLITE3], [sqlite3], ,
	AC_MSG_ERROR(['sqlite3 is required to build xattrfs.'.]))

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h stdlib.h string.h sys/time.h sys/vfs.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_CHECK_FUNCS([fdatasync ftruncate mkdir realpath rmdir strchr strdup utime])

# Debug build
AC_ARG_ENABLE([debug],
	      AC_HELP_STRING([--enable-debug],
			     [Enable debug build options.]))

BUILD_DEBUG=no
if test "x$enable_debug" = "xyes"; then
	BUILD_DEBUG=yes
	CFLAGS=`echo $CFLAGS | sed -e s/O2/O0/`
else
	BUILD_DEBUG=no
fi
AC_SUBST(CFLAGS)

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT

