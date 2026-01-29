#pragma once
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #define STDIN_FILENO _fileno(stdin)
    #define STDOUT_FILENO _fileno(stdout)
    #include <io.h>
    #define FILE_OPEN       _open
    #define FILE_FDOPEN     _fdopen
    #define FILE_CLOSE      _close
    #define FILE_READ       _read
    #define FILE_WRITE      _write
    #include <direct.h>
    #define CHDIR           _chdir
#else   /* !_WIN32 */
    #define FILE_OPEN   open
    #define FILE_FDOPEN fdopen
    #define FILE_CLOSE  close
    #define FILE_READ   read
    #define FILE_WRITE  write
    #define CHDIR       chdir
#endif  /* !_WIN32 */

#ifdef	HAVE_UNISTD_H
#include <unistd.h>
#endif	/* HAVE_UNISTD_H */

#ifdef	HAVE_FCNTL_H
#include <fcntl.h>
#endif	/* HAVE_FCNTL_H */

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif /* HAVE_STDARG_H */

#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif /* HAVE_STDDEF_H */

#ifdef	HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif	/* HAVE_SYS_TYPES_H */

#ifdef	HAVE_SYS_TIME_H
#include <sys/time.h>
#endif	/* HAVE_SYS_TIME_H */

#ifdef	HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif	/* HAVE_SYS_WAIT_H */

#ifdef	HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif	/* HAVE_SYS_PARAM_H */

#ifndef	EDIT_SOURCE
/* all options and configuration */
#include "efuns/options.h"
#endif	/* ! EDIT_SOURCE */

#include "port/wrapper.h"
#include "port/byte_code.h"
#include "port/debug.h"

typedef struct {
    size_t real_size;
    char *buffer;
} outbuffer_t;

void outbuf_zero(outbuffer_t *);
void outbuf_add(outbuffer_t *, const char *);
void outbuf_addchar(outbuffer_t *, char);
void outbuf_addv(outbuffer_t *, const char *, ...);
void outbuf_fix(outbuffer_t *);
void outbuf_push(outbuffer_t *);
size_t outbuf_extend(outbuffer_t *, size_t);

#ifndef NO_OPCODES
#include "efuns_opcode.h"
#endif

#include "stem.h"
