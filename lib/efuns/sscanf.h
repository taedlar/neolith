#pragma once

#include "lpc/types.h"

#ifdef __cplusplus
extern "C" {
#endif

int inter_sscanf(svalue_t *, svalue_t *, svalue_t *, int);

#ifdef F_SSCANF
void f_sscanf(void);
#endif

#ifdef __cplusplus
}
#endif
