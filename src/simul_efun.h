/*  $Id: simul_efun.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith

    ORIGINAL AUTHOR
	Unknown

    MODIFIED BY
	[2001-06-27] by Annihilator <annihilator@muds.net>, see CVS log.
 */

#ifndef SIMUL_EFUN_H
#define SIMUL_EFUN_H

#include "compiler.h"

typedef struct {
    compiler_function_t *func;
    int index;
} simul_info_t;

extern object_t *simul_efun_ob;
extern simul_info_t *simuls;

extern void init_simul_efun(char *);
extern void set_simul_efun(object_t *);
extern int find_simul_efun(char *);

#endif	/* ! SIMUL_EFUN_H */
