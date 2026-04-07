#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lpc/types.h"

int inter_sscanf(svalue_t *, svalue_t *, svalue_t *, int);

#ifdef F_SSCANF
void f_sscanf(void);
#endif

#ifdef __cplusplus
}
#endif
