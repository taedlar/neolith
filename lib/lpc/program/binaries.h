#pragma once
#include "lpc/compiler.h"

void init_binaries();

program_t *load_binary(const char *);

void save_binary(program_t *, mem_block_t *, mem_block_t *);
