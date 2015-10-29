# xattrfs #

A simple FUSE file system, which stores extended attributes in SQLite database. All other file operations are delivered to the underlying base file system.

## Prerequisites ##

* Linux (not tested on other platforms)
* FUSE (fuse-devel)
* SQLite (sqlite3-devel)

## Install ##

```
$ ./autogen.sh
$ ./configure --prefix=/any/dir/you/want
$ make
$ make install
```

It only generates a single executable (xattrfs).

## Usage ##

```
$ xattrfs -h
Usage: xattrfs [OPTIONS].. b:<basedir> <mountpoint>

options:
  -d, --debug           Enable debug mode
  -h, --help            This help message

$ xattrfs b:/source /dest
```

