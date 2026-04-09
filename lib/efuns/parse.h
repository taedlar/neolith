#pragma once

#include "lpc/types.h"

#ifdef __cplusplus
extern "C" {
#endif

int parse(char *, svalue_t *, char *, svalue_t *, int);

#ifdef F_PARSE_COMMAND
void f_parse_command(void);
#endif

#ifdef __cplusplus
}
#endif
