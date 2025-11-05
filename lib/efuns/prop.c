#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "lpc/array.h"
#include "lpc/buffer.h"
#include "lpc/class.h"
#include "lpc/object.h"

#ifdef F_CLASSP
void
f_classp (void)
{
  if (sp->type == T_CLASS)
    {
      free_class (sp->u.arr);
      *sp = const1;
    }
  else
    {
      free_svalue (sp, "f_classp");
      *sp = const0;
    }
}
#endif


#ifdef F_CLONEP
void
f_clonep (void)
{
  if ((sp->type == T_OBJECT) && (sp->u.ob->flags & O_CLONE))
    {
      free_object (sp->u.ob, "f_clonep");
      *sp = const1;
    }
  else
    {
      free_svalue (sp, "f_clonep");
      *sp = const0;
    }
}
#endif


#ifdef F_INTP
void
f_intp (void)
{
  if (sp->type == T_NUMBER)
    sp->u.number = 1;
  else
    {
      free_svalue (sp, "f_intp");
      put_number (0);
    }
}
#endif


#ifdef F_OBJECTP
void
f_objectp (void)
{
  if (sp->type == T_OBJECT)
    {
      free_object (sp->u.ob, "f_objectp");
      *sp = const1;
    }
  else
    {
      free_svalue (sp, "f_objectp");
      *sp = const0;
    }
}
#endif


#ifdef F_POINTERP
void
f_pointerp (void)
{
  if (sp->type == T_ARRAY)
    {
      free_array (sp->u.arr);
      *sp = const1;
    }
  else
    {
      free_svalue (sp, "f_pointerp");
      *sp = const0;
    }
}
#endif


#ifdef F_STRINGP
void
f_stringp (void)
{
  if (sp->type == T_STRING)
    {
      free_string_svalue (sp);
      *sp = const1;
    }
  else
    {
      free_svalue (sp, "f_stringp");
      *sp = const0;
    }
}
#endif


#ifdef F_BUFFERP
void
f_bufferp (void)
{
  if (sp->type == T_BUFFER)
    {
      free_buffer (sp->u.buf);
      *sp = const1;
    }
  else
    {
      free_svalue (sp, "f_bufferp");
      *sp = const0;
    }
}
#endif


#ifdef F_UNDEFINEDP
void
f_undefinedp (void)
{
  if (sp->type == T_NUMBER)
    {
      if (!sp->u.number && (sp->subtype == T_UNDEFINED))
        {
          *sp = const1;
        }
      else
        *sp = const0;
    }
  else
    {
      free_svalue (sp, "f_undefinedp");
      *sp = const0;
    }
}
#endif


#ifdef F_VIRTUALP
void
f_virtualp (void)
{
  int i;

  i = (int) sp->u.ob->flags & O_VIRTUAL;
  free_object (sp->u.ob, "f_virtualp");
  put_number (i != 0);
}
#endif


#ifdef F_FLOATP
void
f_floatp (void)
{
  if (sp->type == T_REAL)
    {
      sp->type = T_NUMBER;
      sp->u.number = 1;
    }
  else
    {
      free_svalue (sp, "f_floatp");
      *sp = const0;
    }
}
#endif

