/*  $Id: rc.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifndef	RC_H
#define	RC_H

#include "lpc/types.h"
#include "lpc/include/runtime_config.h"

#define CONFIG_STR(x)           config_str[(x) - BASE_CONFIG_STR]
#define CONFIG_INT(x)           config_int[(x) - BASE_CONFIG_INT]

extern int config_int[NUM_CONFIG_INTS];
extern char *config_str[NUM_CONFIG_STRS];

extern void init_config(char *);
extern int get_config_item(svalue_t *, svalue_t *);

#endif	/* ! RC_H */
