/*  $Id: operator.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifndef	EFUNS_OPERATOR_H
#define	EFUNS_OPERATOR_H

void f_ge(void);
void f_le(void);
void f_lt(void);
void f_gt(void);
void f_and(void);
void f_and_eq(void);
void f_div_eq(void);
void f_eq(void);
void f_lsh(void);
void f_lsh_eq(void);
void f_mod_eq(void);
void f_mult_eq(void);
void f_ne(void);
void f_or(void);
void f_or_eq(void);
void f_parse_command(void);
void f_range(int);
void f_extract_range(int);
void f_rsh(void);
void f_rsh_eq(void);
void f_simul_efun(void);
void f_sub_eq(void);
void f_switch(void);
void f_xor(void);
void f_xor_eq(void);
void f_function_constructor(void);
void f_evaluate(void);
void f_sscanf(void);

/*
 * eoperators.c
 */
funptr_t *make_funp(svalue_t *, svalue_t *);
void push_funp(funptr_t *);
void free_funp(funptr_t *);
int merge_arg_lists(int, array_t *, int);
void call_simul_efun(unsigned short, int);

funptr_t *make_efun_funp(int, svalue_t *);
funptr_t *make_lfun_funp(int, svalue_t *);
funptr_t *make_simul_funp(int, svalue_t *);

#endif	/* ! EFUNS_OPERATOR_H */
