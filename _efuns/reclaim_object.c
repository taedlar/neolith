/*  $Id: reclaim_object.c,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

/*
 * reclaim.c
 * loops through all variables in all objects looking for the possibility
 * of freeing up destructed objects (that are still hanging around because
 * of references) -- coded by Blackthorn@Genocide Feb. 1993
 */

#include "src/std.h"
#include "_lpc/types.h"
#include "_lpc/array.h"
#include "_lpc/object.h"
#include "_lpc/program.h"
#include "_lpc/mapping.h"
#include "src/main.h"
#include "src/interpret.h"
#include "src/simulate.h"

#include "reclaim_object.h"

#define MAX_RECURSION 25

static void gc_mapping (mapping_t *);
static void check_svalue (svalue_t *);

static int cleaned, nested;

static void
check_svalue (svalue_t * v)
{
  register int idx;

  nested++;
  if (nested > MAX_RECURSION)
    return;

  switch (v->type)
    {
    case T_OBJECT:
      if (v->u.ob->flags & O_DESTRUCTED)
	{
	  free_svalue (v, "reclaim_objects");
	  *v = const0u;
	  cleaned++;
	}
      break;
    case T_MAPPING:
      gc_mapping (v->u.map);
      break;
    case T_ARRAY:
    case T_CLASS:
      for (idx = 0; idx < v->u.arr->size; idx++)
	check_svalue (&v->u.arr->item[idx]);
      break;
    case T_FUNCTION:
      {
	svalue_t tmp;
	tmp.type = T_ARRAY;
	if ((tmp.u.arr = v->u.fp->hdr.args))
	  check_svalue (&tmp);
	break;
      }
    }
  nested--;
  return;
}

static void
gc_mapping (mapping_t * m)
{
  /* Be careful to correctly handle destructed mapping keys.  We can't
   * just call check_svalue() b/c the hash would be wrong and the '0'
   * element we add would be unreferenceable (in most cases)
   */
  mapping_node_t **prev, *elt;
  int j = (int) m->table_size;

  do
    {
      prev = m->table + j;
      while ((elt = *prev))
	{
	  if (elt->values[0].type == T_OBJECT)
	    {
	      if (elt->values[0].u.ob->flags & O_DESTRUCTED)
		{
		  free_object (elt->values[0].u.ob, "gc_mapping");

		  /* found one, do a map_delete() */
		  if (!(*prev = elt->next) && !m->table[j])
		    m->unfilled++;
		  cleaned++;
		  m->count--;
		  total_mapping_nodes--;
		  total_mapping_size -= sizeof (mapping_node_t);
		  free_svalue (elt->values + 1, "gc_mapping");
		  free_node (elt);
		  continue;
		}
	    }
	  else
	    {
	      /* in case the key is a mapping or something */
	      check_svalue (elt->values);
	    }
	  check_svalue (elt->values + 1);
	  prev = &(elt->next);
	}
    }
  while (j--);
}

int
reclaim_objects ()
{
  int i;
  object_t *ob;

  cleaned = nested = 0;
  for (ob = obj_list; ob; ob = ob->next_all)
    if (ob->prog)
      for (i = 0; i < (int) ob->prog->num_variables_total; i++)
	check_svalue (&ob->variables[i]);
  return cleaned;
}

#ifdef F_RECLAIM_OBJECTS
void
f_reclaim_objects (void)
{
  push_number (reclaim_objects ());
}
#endif
