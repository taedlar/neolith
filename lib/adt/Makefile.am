AM_CFLAGS = -Wall -ggdb -O2 -pipe
AM_CPPFLAGS = -DLOCALEDIR=\"@localedir@\" @DEFS@ -I@top_srcdir@ -I@top_srcdir@/lib

noinst_LTLIBRARIES = libadt.la

libadt_la_SOURCES = hash.c hash.h avltree.c avltree.h crc32.c crc32.h \
	qsort.c qsort.h
