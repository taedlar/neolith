/*  $Id: binaries.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif	/* HAVE_CONFIG_H */

#ifndef BINARIES_H
#define BINARIES_H

#include "compiler.h"

FILE *crdir_fopen(char *);
void init_binaries();
#define load_binary(x, y) int_load_binary(x)
program_t *int_load_binary(char *);
void save_binary(program_t *, mem_block_t *, mem_block_t *);

#endif	/* BINARIES_H */
