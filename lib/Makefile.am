SUBDIRS = adt efuns lpc socket .

pkglib_LTLIBRARIES = libneolith.la

libneolith_la_SOURCES = misc.c wrapper.c wrapper.h
libneolith_la_LIBADD = \
	adt/libadt.la \
	efuns/libefuns.la \
	lpc/liblpc.la \
	socket/libsocket.la
