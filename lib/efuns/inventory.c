#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "lpc/object.h"
#include "lpc/array.h"


#ifdef F_ENVIRONMENT
void
f_environment (void)
{
  object_t *ob;

  if (st_num_arg)
    {
      if ((ob = sp->u.ob)->flags & O_DESTRUCTED)
        ob = 0;
      else
        {
          ob = ob->super;
          free_object ((sp--)->u.ob, "f_environment");
        }
    }
  else
    {
      if (!(current_object->flags & O_DESTRUCTED))
        ob = current_object->super;
      else
        ob = 0;
    }

  if (ob)
    push_object (ob);
  else
    push_number (0);
}
#endif


#ifdef F_MOVE_OBJECT
void
f_move_object (void)
{
  object_t *o1, *o2;

  /* get destination */
  if (sp->type == T_OBJECT)
    o2 = sp->u.ob;
  else
    {
      if (!(o2 = find_or_load_object (sp->u.string)) || !object_visible (o2))
        error ("move_object failed: could not find destination\n");
    }

  if ((o1 = current_object)->flags & O_DESTRUCTED)
    error ("move_object(): can't move a destructed object\n");

  move_object (o1, o2);
  pop_stack ();
}
#endif


#ifdef F_PRESENT
void
f_present (void)
{
  object_t *ob;
  int num_arg = st_num_arg;
  svalue_t *arg = sp - num_arg + 1;

#ifdef LAZY_RESETS
  if (num_arg == 2)
    {
      try_reset (arg[1].u.ob);
    }
#endif
  ob = object_present (arg, num_arg == 1 ? 0 : arg[1].u.ob);
  pop_n_elems (num_arg);
  if (ob)
    push_object (ob);
  else
    *++sp = const0;
}
#endif


#ifdef F_FIRST_INVENTORY
void
f_first_inventory (void)
{
  object_t *ob;
  ob = first_inventory (sp);
  free_svalue (sp, "f_first_inventory");
  if (ob)
    {
      put_unrefed_undested_object (ob, "first_inventory");
    }
  else
    *sp = const0;
}
#endif


#ifdef F_NEXT_INVENTORY
void
f_next_inventory (void)
{
  object_t *ob;

  ob = sp->u.ob->next_inv;
  free_object (sp->u.ob, "f_next_inventory");
  while (ob)
    {
      if (ob->flags & O_HIDDEN)
        {
          object_t *old_ob = ob;
          if (object_visible (ob))
            {
              add_ref (old_ob, "next_inventory(ob) : 1");
              sp->u.ob = old_ob;
              return;
            }
        }
      else
        {
          add_ref (ob, "next_inventory(ob) : 2");
          sp->u.ob = ob;
          return;
        }
      ob = ob->next_inv;
    }
  *sp = const0;
}
#endif


#ifdef F_ALL_INVENTORY
void
f_all_inventory (void)
{
  array_t *vec = all_inventory (sp->u.ob, 0);
  free_object (sp->u.ob, "f_all_inventory");
  sp->type = T_ARRAY;
  sp->u.arr = vec;
}
#endif


#ifdef F_DEEP_INVENTORY
void
f_deep_inventory (void)
{
  array_t *vec;

  vec = deep_inventory (sp->u.ob, 0);
  free_object (sp->u.ob, "f_deep_inventory");
  put_array (vec);
}
#endif

