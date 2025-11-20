#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "src/interpret.h"
#include "lpc/array.h"
#include "lpc/object.h"
#include "lpc/include/origin.h"

/*
 * Call a function in all objects in an array.
 */
static array_t *call_all_other (array_t * v, const char *func, int numargs)
{
  int size;
  svalue_t *tmp, *vptr, *rptr;
  array_t *ret;
  object_t *ob;
  int i;

  tmp = sp;
  (++sp)->type = T_ARRAY;
  sp->u.arr = ret = allocate_array (size = v->size);
  if (size)
    STACK_CHECK (numargs);

  for (vptr = v->item, rptr = ret->item; size--; vptr++, rptr++)
    {
      if (vptr->type == T_OBJECT)
        {
          ob = vptr->u.ob;
        }
      else if (vptr->type == T_STRING)
        {
          ob = find_or_load_object (vptr->u.string);
          if (!ob || !object_visible (ob))
            continue;
        }
      else
        continue;
      if (ob->flags & O_DESTRUCTED)
        continue;
      i = numargs;
      while (i--)
        push_svalue (tmp - i);
      call_origin = ORIGIN_CALL_OTHER;
      if (apply_low (func, ob, numargs))
        *rptr = *sp--;
    }
  sp--;
  pop_n_elems (numargs);
  return ret;
}

#ifdef F_CALL_OTHER
 /* enhanced call_other written 930314 by Luke Mewburn <zak@rmit.edu.au> */
void
f_call_other (void)
{
  svalue_t *arg;
  object_t *ob;
  char *funcname;
  int num_arg = st_num_arg;

  if (current_object->flags & O_DESTRUCTED)
    {
      pop_n_elems (num_arg);
      push_undefined ();
      return;
    }

  arg = sp - num_arg + 1;
  if (arg[1].type == T_STRING)
    funcname = arg[1].u.string;
  else
    {				/* must be T_ARRAY then */
      array_t *v = arg[1].u.arr;

      check_for_destr (v);
      if ((v->size < 1) || !(v->item->type == T_STRING))
        error ("call_other: 1st elem of array for arg 2 must be a string\n");
      funcname = v->item->u.string;
      num_arg = 2 + merge_arg_lists (num_arg - 2, v, 1);
    }

  if (arg[0].type == T_OBJECT)
    ob = arg[0].u.ob;
  else if (arg[0].type == T_ARRAY)
    {
      array_t *ret;

      ret = call_all_other (arg[0].u.arr, funcname, num_arg - 2);
      pop_stack ();
      free_array (arg->u.arr);
      sp->u.arr = ret;
      return;
    }
  else
    {
      object_t *old_ob;
      ob = find_or_load_object (arg[0].u.string);
      if (!(old_ob = ob) || !object_visible (ob))
        error ("call_other() couldn't find object\n");
      ob = old_ob;
    }

  /* Send the remaining arguments to the function. */
  call_origin = ORIGIN_CALL_OTHER;
  if (apply_low (funcname, ob, num_arg - 2) == 0)
    {				/* Function not found */
      pop_2_elems ();
      push_undefined ();
      return;
    }

  /*
   * The result of the function call is on the stack.  So is the function
   * name and object that was called, though. These have to be removed.
   */
  free_svalue (--sp, "f_call_other:1");
  free_svalue (--sp, "f_call_other:2");
  *sp = *(sp + 2);
  return;
}
#endif

#ifdef F_ORIGIN
void
f_origin (void)
{
  push_constant_string (origin_name (caller_type));
}
#endif
