/*  $Id: dumpstat.c,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "lpc/types.h"
#include "lpc/array.h"
#include "lpc/buffer.h"
#include "lpc/mapping.h"
#include "lpc/object.h"
#include "lpc/program.h"
#include "LPC/function.h"
#include "src/interpret.h"
#include "src/main.h"
#include "src/simulate.h"
#include "src/comm.h"
#include "src/file.h"

/*
 * Write statistics about objects on file.
 */

static int sumSizes (mapping_t *, mapping_node_t *, void *);
static int svalue_size (svalue_t *);

static int
sumSizes (mapping_t * m, mapping_node_t * elt, void *tp)
{
  int *t = (int *) tp;

  *t += (svalue_size (&elt->values[0]) + svalue_size (&elt->values[1]));
  *t += sizeof (mapping_node_t);
  return 0;
}

static int
svalue_size (svalue_t * v)
{
  int i, total;

  switch (v->type)
    {
    case T_OBJECT:
    case T_REAL:
    case T_NUMBER:
      return 0;
    case T_STRING:
      return (int) (strlen (v->u.string) + 1);
    case T_ARRAY:
    case T_CLASS:
      /* first svalue is stored inside the array struct */
      total = sizeof (array_t) - sizeof (svalue_t);
      for (i = 0; i < v->u.arr->size; i++)
	{
	  total += svalue_size (&v->u.arr->item[i]) + sizeof (svalue_t);
	}
      return total;
    case T_MAPPING:
      total = sizeof (mapping_t);
      mapTraverse (v->u.map, sumSizes, &total);
      return total;
    case T_FUNCTION:
      {
	svalue_t tmp;
	tmp.type = T_ARRAY;
	tmp.u.arr = v->u.fp->hdr.args;

	if (tmp.u.arr)
	  total = (int) (sizeof (funptr_hdr_t) + svalue_size (&tmp));
	else
	  total = (int) (sizeof (funptr_hdr_t));
	switch (v->u.fp->hdr.type)
	  {
	  case FP_EFUN:
	    total += sizeof (efun_ptr_t);
	    break;
	  case FP_LOCAL | FP_NOT_BINDABLE:
	    total += sizeof (local_ptr_t);
	    break;
	  case FP_SIMUL:
	    total += sizeof (simul_ptr_t);
	    break;
	  case FP_FUNCTIONAL:
	  case FP_FUNCTIONAL | FP_NOT_BINDABLE:
	    total += sizeof (functional_t);
	    break;
	  }
	return total;
      }
    case T_BUFFER:
      /* first byte is stored inside the buffer struct */
      return (int) (sizeof (buffer_t) + v->u.buf->size - 1);
    case T_ANY:
      break;
    default:
      fatal ("Illegal type: %d\n", v->type);
    }
  /* NOTREACHED */
  return 0;
}

int
data_size (object_t * ob)
{
  int total = 0, i;

  if (ob->prog)
    {
      for (i = 0; i < (int) ob->prog->num_variables_total; i++)
	{
	  total += svalue_size (&ob->variables[i]) + sizeof (svalue_t);
	}
    }
  return total;
}

void
dumpstat (char *tfn)
{
  FILE *f;
  object_t *ob;
  char *fn;
  int display_hidden;

  fn = check_valid_path (tfn, current_object, "dumpallobj", 1);
  if (!fn)
    {
      error ("Invalid path '/%s' for writing.\n", fn);
      return;
    }
  f = fopen (fn, "w");
  if (!f)
    {
      error ("Unable to open '/%s' for writing.\n", fn);
      return;
    }

  display_hidden = -1;
  for (ob = obj_list; ob; ob = ob->next_all)
    {
      int tmp;

      if (ob->flags & O_HIDDEN)
	{
	  if (display_hidden == -1)
	    display_hidden = valid_hide (current_object);
	  if (!display_hidden)
	    continue;
	}
      if (ob->prog && (ob->prog->ref == 1 || !(ob->flags & O_CLONE)))
	tmp = ob->prog->total_size;
      else
	tmp = 0;
      fprintf (f, "%-20s %lu ref %2d %s %s (%d)\n", ob->name,
	       tmp + data_size (ob) + sizeof (object_t), ob->ref,
	       ob->flags & O_HEART_BEAT ? "HB" : "  ",
	       ob->super ? ob->super->name : "--",
	       /* ob->cpu */ 0);
    }
  fclose (f);
}


#ifdef F_DUMPALLOBJ
void
f_dumpallobj (void)
{
  if (st_num_arg)
    {
      dumpstat (sp->u.string);
      free_string_svalue (sp--);
    }
  else
    {
      dumpstat ("/OBJ_DUMP");
    }
}
#endif
