#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "src/call_out.h"
#include "lpc/object.h"

#ifdef F_CALL_OUT
void f_call_out (void) {
  svalue_t *arg = sp - st_num_arg + 1;
  int num = st_num_arg - 2;
#ifdef CALLOUT_HANDLES
  int ret;

  if (!(current_object->flags & O_DESTRUCTED))
    {
      ret = new_call_out (current_object, arg, (time_t)arg[1].u.number, num, arg + 2);
      /* args have been transfered; don't free them;
         also don't need to free the int */
      sp -= num + 1;
    }
  else
    {
      ret = 0;
      pop_n_elems (num);
      sp--;
    }
  /* the function */
  free_svalue (sp, "call_out");
  put_number (ret);
#else
  if (!(current_object->flags & O_DESTRUCTED))
    {
      new_call_out (current_object, arg, (time_t)arg[1].u.number, num, arg + 2);
      sp -= num + 1;
    }
  else
    {
      pop_n_elems (num);
      sp--;
    }
  free_svalue (sp--, "call_out");
#endif
}
#endif


#ifdef F_FIND_CALL_OUT
void f_find_call_out (void) {
  int i;
#ifdef CALLOUT_HANDLES
  if (sp->type == T_NUMBER)
    {
      i = find_call_out_by_handle ((int)sp->u.number);
    }
  else
    {				/* T_STRING */
#endif
      i = find_call_out (current_object, SVALUE_STRPTR(sp));
      free_string_svalue (sp);
#ifdef CALLOUT_HANDLES
    }
#endif
  put_number (i);
}
#endif


#ifdef F_REMOVE_CALL_OUT
void f_remove_call_out (void) {
  int i;

  if (st_num_arg)
    {
#ifdef CALLOUT_HANDLES
      if (sp->type == T_STRING)
        {
#endif
          i = remove_call_out (current_object, SVALUE_STRPTR(sp));
          free_string_svalue (sp);
#ifdef CALLOUT_HANDLES
        }
      else
        {
          i = remove_call_out_by_handle ((int)sp->u.number);
        }
#endif
    }
  else
    {
      remove_all_call_out (current_object);
      i = 0;
      sp++;
    }
  put_number (i);
}
#endif


#ifdef F_CALL_OUT_INFO
void f_call_out_info (void) {
  push_refed_array (get_all_call_outs ());
}
#endif
