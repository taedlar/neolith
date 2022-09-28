/*  $Id: class.c,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

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

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "types.h"
#include "array.h"
#include "class.h"
#include "src/interpret.h"
#include "src/main.h"

void
dealloc_class (array_t * p)
{
  int i;

  for (i = p->size; i--;)
    free_svalue (&p->item[i], "dealloc_class");
  FREE ((char *) p);
}

void
free_class (array_t * p)
{
  if (--(p->ref) > 0)
    return;

  dealloc_class (p);
}

array_t *
allocate_class (class_def_t * cld, int has_values)
{
  array_t *p;
  int n = cld->size;

  p =
    (array_t *) DXALLOC (sizeof (array_t) + sizeof (svalue_t) * (n - 1),
			 TAG_CLASS, "allocate_class");
  p->ref = 1;
  p->size = n;
  if (has_values)
    {
      while (n--)
	p->item[n] = *sp--;
    }
  else
    {
      while (n--)
	p->item[n] = const0;
    }
  return p;
}

array_t *
allocate_class_by_size (int size)
{
  array_t *p;

  p =
    (array_t *) DXALLOC (sizeof (array_t) + sizeof (svalue_t) * (size - 1),
			 TAG_CLASS, "allocate_class");
  p->ref = 1;
  p->size = size;

  while (size--)
    p->item[size] = const0;

  return p;
}
