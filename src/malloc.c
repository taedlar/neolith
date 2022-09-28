/*  $Id: malloc.c,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#define IN_MALLOC_WRAPPER
#define NO_OPCODES
#include "std.h"

#ifdef DO_MSTATS
void
show_mstats (outbuffer_t * ob, char *s)
{
  outbuf_add (ob, "No malloc statistics available with SYSMALLOC\n");
}
#endif
