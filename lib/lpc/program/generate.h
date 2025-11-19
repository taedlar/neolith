#pragma once
#include "lpc/program.h"
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
