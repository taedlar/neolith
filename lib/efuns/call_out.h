#pragma once
#include "lpc/types.h"

void call_out(void);
int find_call_out_by_handle(int);
int remove_call_out_by_handle(int);
int new_call_out(object_t *, svalue_t *, time_t, int, svalue_t *);
int remove_call_out(object_t *, char *);
void remove_all_call_out(object_t *);
int find_call_out(object_t *, char *);
array_t *get_all_call_outs(void);
int print_call_out_usage(outbuffer_t *, int);
void mark_call_outs(void);
