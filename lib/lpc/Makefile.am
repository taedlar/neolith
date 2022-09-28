AM_CFLAGS = -Wall -ggdb -O2 -pipe
AM_CPPFLAGS = -I@top_srcdir@/lib -I@top_srcdir@/lib/adt

noinst_LTLIBRARIES = liblpc.la

liblpc_la_SOURCES = buffer.c buffer.h class.c class.h mapping.c mapping.h \
	array.c array.h object.c object.h program.c program.h uids.c uids.h \
	function.h

noinst_HEADERS = types.h
