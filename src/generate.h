/*  $Id: generate.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifndef GENERATE_H
#define GENERATE_H

#include "trees.h"
#include "program.h"
#include "icode.h"

#define generate_function_call i_generate_function_call
#define generate_inherited_init_call i_generate_inherited_init_call
#define generate___INIT i_generate___INIT
#define generate_final_program i_generate_final_program
#define initialize_parser i_initialize_parser

int node_always_true(parse_node_t *);
short generate(parse_node_t *);
short generate_function(compiler_function_t *, parse_node_t *, int);
int generate_conditional_branch(parse_node_t *);

#endif	/* ! GENERATE_H */
