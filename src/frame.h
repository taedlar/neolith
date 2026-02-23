#pragma once

#include "lpc/types.h"
#include "lpc/array.h"
#include "lpc/program.h"

compiler_function_t *setup_new_frame(int runtime_index);
compiler_function_t *setup_inherited_frame(int runtime_index);
void setup_variables (int actual, int local, int num_arg);
void setup_varargs_variables (int actual, int local, int num_arg);
void pop_control_stack(void);
void push_control_stack(int);
void do_catch (const char *, unsigned short);

char* get_line_number (const char *p, const program_t * progp);
void get_line_number_info (char **ret_file, int *ret_line);
char *dump_trace (int);
array_t *get_svalue_trace (int);
