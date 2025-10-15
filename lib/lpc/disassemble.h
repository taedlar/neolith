#pragma once

#include <stdio.h>

#include "types.h"

extern void disassemble (FILE *, char *, int, int, program_t *);
extern void dump_line_numbers (FILE *, program_t *);
