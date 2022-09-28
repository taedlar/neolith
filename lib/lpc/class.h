/*  $Id: class.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith

    ORIGINAL AUTHOR
	Unknown

    MODIFIED BY
	[2001-06-27] by Annihilator <annihilator@muds.net>
 */

#ifndef _LPC_CLASS_H
#define _LPC_CLASS_H

#include "src/compiler.h"

void dealloc_class(array_t *);
void free_class(array_t *);
array_t *allocate_class(class_def_t *, int);
array_t *allocate_class_by_size(int);

#endif	/* ! _LPC_CLASS_H */
