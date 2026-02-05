#pragma once
#include "types.h"

void free_svalue (svalue_t * v, const char* caller);
void free_some_svalues(svalue_t *, int);
void assign_svalue(svalue_t *, svalue_t *);
void assign_svalue_no_free(svalue_t *, svalue_t *);
void copy_some_svalues(svalue_t *, svalue_t *, int);
