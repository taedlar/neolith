AM_CFLAGS = -Wall -ggdb -O2 -pipe
AM_CPPFLAGS = -DLOCALEDIR=\"@localedir@\" @DEFS@ -I@top_srcdir@ -I@top_srcdir@/lib -I@top_srcdir@/lib/adt

noinst_PROGRAMS = edit_source

edit_source_SOURCES = edit_source.c edit_source.h make_func.y
edit_source_LDADD = ../adt/libadt.la ../port/libport.la

BUILT_SOURCES = efuns_option.h efuns_opcode.h efuns_vector.h efuns_prototype.h efuns_definition.h

$(BUILT_SOURCES): edit_source prototype.c
	${CPP} -I@srcdir@ -x c-header @srcdir@/prototype.c > prototype.i; \
	[ -x ./edit_source ] && ./edit_source @srcdir@/options.h prototype.i; \
	${RM} prototype.i

noinst_LTLIBRARIES = libefuns.la
libefuns_la_SOURCES = $(BUILT_SOURCES) \
	operator.c operator.h math.c unsorted.c file.c debug.c \
	file_utils.c file_utils.h replace_program.c replace_program.h \
	call_out.c call_out.h string.c datetime.c \
	reclaim_object.c reclaim_object.h \
	sprintf.c sprintf.h \
	dump_prog.c parse.c parse.h \
	ed.c ed.h dumpstat.c dumpstat.h

EXTRA_DIST = prototype.lpc options.h
