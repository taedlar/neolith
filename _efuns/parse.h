/*  $Id: parse.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

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

#ifndef _EFUNS_PARSE_H
#define _EFUNS_PARSE_H

#include "_lpc/types.h"

int parse(char *, svalue_t *, char *, svalue_t *, int);
char *process_string(char *);
svalue_t *process_value(char *);
char *break_string(char *, int, svalue_t *);

#endif	/* ! _EFUNS_PARSE_H */
