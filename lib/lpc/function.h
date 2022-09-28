/*  $Id: function.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifndef _LPC_FUNCTION_H
#define _LPC_FUNCTION_H

#include "types.h"

/* FP_LOCAL */
typedef struct {
    short index;
} local_ptr_t;

/* FP_SIMUL */
typedef local_ptr_t simul_ptr_t;

/* FP_EFUN */
typedef local_ptr_t efun_ptr_t;

/* FP_FUNCTIONAL */
typedef struct {
    /* these two must come first */
    unsigned char num_arg;
    unsigned char num_local;
    short offset;
    program_t *prog;
    short fio, vio;
} functional_t;

/* common header */
typedef struct {
    unsigned short ref;
    short type;                 /* FP_* is used */
    struct object_s *owner;
    struct array_s *args;
} funptr_hdr_t;

struct funptr_s {
    funptr_hdr_t hdr;
    union {
	efun_ptr_t efun;
	local_ptr_t local;
	simul_ptr_t simul;
	functional_t functional;
    } f;
};

union string_or_func_u {
    funptr_t *f;
    char *s;
};

void dealloc_funp(funptr_t *);
void push_refed_funp(funptr_t *);

#endif	/* ! _LPC_FUNCTION_H */
