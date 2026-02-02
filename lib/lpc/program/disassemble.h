#pragma once

#include <stdio.h>

#include "lpc/types.h"

extern void disassemble (FILE *, char *, ptrdiff_t, ptrdiff_t, program_t *);
extern void dump_line_numbers (FILE *, program_t *);
