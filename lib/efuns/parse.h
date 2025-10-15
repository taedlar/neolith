#pragma once
#include "lpc/types.h"

int parse(char *, svalue_t *, char *, svalue_t *, int);
char *process_string(char *);
svalue_t *process_value(char *);
char *break_string(char *, int, svalue_t *);
