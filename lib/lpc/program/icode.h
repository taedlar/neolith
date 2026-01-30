#pragma once

#include "parse_trees.h"

#ifndef _WIN32
typedef unsigned char BYTE;
#endif

void i_generate___INIT(void);
void i_generate_node(parse_node_t *);
void i_generate_continue(void);
void i_generate_forward_jump(void);
void i_update_forward_jump(void);
void i_update_continues(void);
void i_branch_backwards(BYTE kind, ptrdiff_t addr);
void i_update_breaks(void);
void i_save_loop_info(parse_node_t *);
void i_restore_loop_info(void);
void i_generate_forward_branch(BYTE);
void i_update_forward_branch(void);
void i_update_forward_branch_links(BYTE kind, parse_node_t* link_start);
void i_generate_else(void);
void i_initialize_parser(void);
void i_generate_final_program(int);
void i_generate_inherited_init_call(int, short);

void optimize_icode(char *, char *, char *);
