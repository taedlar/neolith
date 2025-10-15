#pragma once
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
#include "efuns/options.h"
#endif	/* ! EDIT_SOURCE */

#include "port/wrapper.h"
#include "port/byte_code.h"
#include "port/debug.h"

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
#include "efuns_opcode.h"
#endif

#include "stem.h"
