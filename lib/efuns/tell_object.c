#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "file_utils.h"
#include "lpc/array.h"
#include "lpc/object.h"

#ifdef F_TELL_OBJECT
void
f_tell_object (void)
{
  tell_object ((sp - 1)->u.ob, sp->u.string);
  free_string_svalue (sp--);
  pop_stack ();
}
#endif


#ifdef F_WRITE
void
f_write (void)
{
  do_write (sp);
  pop_stack ();
}
#endif


#ifdef F_TELL_ROOM
void
f_tell_room (void)
{
  object_t *ob;
  array_t *avoid;
  int num_arg = st_num_arg;
  svalue_t *arg = sp - num_arg + 1;

  if (arg->type == T_OBJECT)
    {
      ob = arg[0].u.ob;
    }
  else
    {				/* must be a string... */
      ob = find_or_load_object (arg[0].u.string);
      if (!ob || !object_visible (ob))
        error ("Bad argument 1 to tell_room()\n");
    }

  if (num_arg == 2)
    {
      avoid = &the_null_array;
    }
  else
    {
      avoid = arg[2].u.arr;
    }

  tell_room (ob, &arg[1], avoid);
  free_array (avoid);
  free_svalue (arg + 1, "f_tell_room");
  free_svalue (arg, "f_tell_room");
  sp = arg - 1;
}
#endif


#ifdef F_SAY
void
f_say (void)
{
  array_t *avoid;
  static array_t vtmp = { .ref = 1, 1, };

  if (st_num_arg == 1)
    {
      avoid = &the_null_array;
      say (sp, avoid);
      pop_stack ();
    }
  else
    {
      if (sp->type == T_OBJECT)
        {
          vtmp.item[0].type = T_OBJECT;
          vtmp.item[0].u.ob = sp->u.ob;
          avoid = &vtmp;
        }
      else
        {			/* must be a array... */
          avoid = sp->u.arr;
        }
      say (sp - 1, avoid);
      pop_2_elems ();
    }
}
#endif


#ifdef F_SHOUT
void
f_shout (void)
{
  shout_string (sp->u.string);
  free_string_svalue (sp--);
}
#endif


#ifdef F_TAIL
void
f_tail (void)
{
  if (tail (sp->u.string))
    {
      free_string_svalue (sp);
      *sp = const1;
    }
  else
    {
      free_string_svalue (sp);
      *sp = const0;
    }
}
#endif

