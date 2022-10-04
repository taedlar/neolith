#ifndef LPC_DISASSEMBLE_H
#define LPC_DISASSEMBLE_H

#ifdef STDC_HEADERS
#include <stdio.h>
#endif /* STDC_HEADERS */

#include "types.h"

extern void disassemble (FILE *, char *, int, int, program_t *);
extern void dump_line_numbers (FILE *, program_t *);

#endif /* ! LPC_DISASSEMBLE_H */

