#pragma once
#include "lpc/compiler.h"

void init_binaries();

#define load_binary(x, y) int_load_binary(x)
program_t *int_load_binary(const char *);

void save_binary(program_t *, mem_block_t *, mem_block_t *);
