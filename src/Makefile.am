AM_CFLAGS = -Wall -ggdb -O -pipe
AM_CPPFLAGS = -DLOCALEDIR=\"@localedir@\" @DEFS@ -I@top_srcdir@ -I@top_srcdir@/lib -I@top_srcdir@/lib/adt -I@top_builddir@/lib/efuns
AM_YFLAGS = --defines=@builddir@/grammar.h

EXTRA_DIST = preprocess.c ChangeLog neolith.conf

##########################################################################
# Binaries to make

bin_PROGRAMS = neolith

neolith_SOURCES = grammar.y lex.c main.c rc.c comm.c backend.c \
	interpret.c program.c compiler.c icode.c trees.c \
	generate.c scratchpad.c binaries.c malloc.c \
	simulate.c stralloc.c regexp.c simul_efun.c uids.c

neolith_LDADD = ../lib/efuns/libefuns.la ../lib/lpc/liblpc.la ../lib/socket/libsocket.la ../lib/adt/libadt.la ../lib/port/libport.la -lm -lcrypt

noinst_HEADERS = applies.h backend.h binaries.h comm.h compiler.h \
	generate.h icode.h interpret.h lex.h \
	stem.h main.h malloc.h otable.h preprocess.h program.h rc.h regexp.h \
	scratchpad.h simul_efun.h simulate.h std.h stralloc.h trees.h uids.h
