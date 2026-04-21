#pragma once

#define LPCBIN_MAGIC "NEOL"
#define LPCBIN_DRIVER_ID 0x20260418

#define BIN_IGNORE_SOURCE_FILE 0x1 /* ignore source file when checking binary validity */

#ifdef __cplusplus
extern "C" {
#endif
#include "lpc/compiler.h"

void init_binaries();
program_t *load_binary (const char *, unsigned long flags);
void save_binary(program_t *, mem_block_t *, mem_block_t *);

#ifdef __cplusplus
}
#endif
