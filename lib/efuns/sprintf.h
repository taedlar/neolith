#pragma once

#define SV2STR_NOINDENT		0x0001	/* don't generate indention */
#define SV2STR_DONEINDENT	0x0002	/* indent of this line is already done */
#define SV2STR_NONEWLINE	0x0004	/* don't generate newline */

void svalue_to_string(svalue_t *, outbuffer_t *, int, char, int);
char *string_print_formatted(char *, int, svalue_t *);
