YFLAGS = -d
AM_CFLAGS = -Wall -ggdb -O -pipe
AM_CPPFLAGS = -DLOCALEDIR=\"@localedir@\" @DEFS@ -I@top_srcdir@/lib -I@top_srcdir@/lib/adt

EXTRA_DIST = preprocess.c ChangeLog neolith.conf

##########################################################################
# Binaries to make

bin_PROGRAMS = neolith

neolith_SOURCES = grammar.y grammar.h lex.c main.c rc.c comm.c backend.c \
	interpret.c logger.c otable.c simulate.c compiler.c icode.c trees.c \
	generate.c scratchpad.c binaries.c file.c malloc.c \
	stralloc.c regexp.c simul_efun.c

neolith_LDADD = ../lib/libneolith.la -lm -lcrypt

noinst_HEADERS = applies.h backend.h binaries.h comm.h compiler.h \
	file.h generate.h icode.h interpret.h lex.h logger.h \
	macros.h main.h malloc.h otable.h preprocess.h rc.h regexp.h \
	scratchpad.h simul_efun.h simulate.h std.h stralloc.h trees.h
