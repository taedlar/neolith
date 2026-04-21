#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "lpc/compiler.h"

void init_binaries();
void refresh_binaries_config_id();
uint64_t compute_binaries_config_id();
program_t *load_binary (const char *);
void save_binary(program_t *, mem_block_t *, mem_block_t *);

#ifdef __cplusplus
}
#endif
