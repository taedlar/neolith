/*  $Id: call_out.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifndef CALL_OUT_H
#define CALL_OUT_H

#include "lpc/types.h"

void call_out(void);
int find_call_out_by_handle(int);
int remove_call_out_by_handle(int);
int new_call_out(object_t *, svalue_t *, int, int, svalue_t *);
int remove_call_out(object_t *, char *);
void remove_all_call_out(object_t *);
int find_call_out(object_t *, char *);
array_t *get_all_call_outs(void);
int print_call_out_usage(outbuffer_t *, int);
void mark_call_outs(void);

#endif	/* ! CALL_OUT_H */
