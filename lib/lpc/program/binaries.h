#pragma once
#include <stdio.h>
#include "lpc/compiler.h"

FILE *crdir_fopen(char *);
void init_binaries();
#define load_binary(x, y) int_load_binary(x)
program_t *int_load_binary(char *);
void save_binary(program_t *, mem_block_t *, mem_block_t *);
