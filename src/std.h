/*  $Id: std.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifndef STD_H
#define STD_H

#ifdef	STDC_HEADERS
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#endif	/* STDC_HEADERS */

#ifdef	HAVE_UNISTD_H
#include <unistd.h>
#endif	/* HAVE_UNISTD_H */

#ifdef	HAVE_FCNTL_H
#include <fcntl.h>
#endif	/* HAVE_FCNTL_H */

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
#include "_efuns/options.h"
//#include "configure.h"
#endif	/* ! EDIT_SOURCE */

#include "macros.h"

typedef struct {
    int real_size;
    char *buffer;
} outbuffer_t;

void outbuf_zero(outbuffer_t *);
void outbuf_add(outbuffer_t *, char *);
void outbuf_addchar(outbuffer_t *, char);
void outbuf_addv(outbuffer_t *, char *, ...);
void outbuf_fix(outbuffer_t *);
void outbuf_push(outbuffer_t *);
int outbuf_extend(outbuffer_t *, int);

#ifndef NO_OPCODES
#include "_efuns/.opcode.h"
#endif

#endif	/* ! STD_H */
