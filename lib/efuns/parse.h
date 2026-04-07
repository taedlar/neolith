#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "lpc/types.h"

int parse(char *, svalue_t *, char *, svalue_t *, int);

#ifdef F_PARSE_COMMAND
void f_parse_command(void);
#endif

#ifdef __cplusplus
}
#endif
