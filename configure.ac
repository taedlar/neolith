AC_INIT([neolith], [0.1.3])
AC_CONFIG_SRCDIR([gettext.h])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE
AC_USE_SYSTEM_EXTENSIONS

AH_TOP([#define debug(x,y)
])

AH_TEMPLATE([ENABLE_NLS],
	    [Enables i18n of string messages with GNU gettext.])
AH_TEMPLATE([HAVE_GETTEXT],
	    [Defined when GNU gettext is available.])

AH_BOTTOM([#if defined(ENABLE_NLS) && defined(HAVE_GETTEXT)
#include <gettext.h>
#define _(s) gettext(s)
#else
#define _(s) (s)
#endif
])

AC_PROG_CC
AC_PROG_YACC
AM_PROG_LIBTOOL

ALL_LINGUAS="zh_TW"
AM_GNU_GETTEXT([external])

localedir=$datadir/locale
AC_SUBST(localedir)

AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_MAJOR
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([argp.h fcntl.h malloc.h sys/ioctl.h sys/time.h unistd.h\
	stdint.h sys/inttypes.h arpa/telnet.h netdb.h socket.h sys/socket.h\
	sys/types.h sys/param.h])

AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_STRUCT_ST_RDEV
AC_HEADER_TIME
AC_STRUCT_TM
AC_STRUCT_TIMEZONE

AC_PROG_GCC_TRADITIONAL
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_FUNC_WAIT3
AC_CHECK_FUNCS(getcwd gethostname getline gettimeofday getwd mkdir putenv\
	regcomp rmdir select socket stpcpy strcspn strerror strstr strtol\
	ualarm)

AC_OUTPUT([ po/Makefile.in
  $PACKAGE.spec:spec.in
  Makefile
  _adt/Makefile
  _efuns/Makefile
  _lpc/Makefile
  _socket/Makefile
  lib/Makefile
  src/Makefile
  LPC/Makefile
])
