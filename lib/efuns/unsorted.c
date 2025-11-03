#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/resource.h>

#include "src/std.h"
#include "crc32.h"
#include "rc.h"
#include "file_utils.h"
#include "parse.h"
#include "dumpstat.h"
#include "src/interpret.h"
#include "lpc/object.h"
#include "lpc/array.h"
#include "lpc/buffer.h"
#include "lpc/class.h"
#include "lpc/mapping.h"
#include "lpc/compiler.h"
#include "lpc/otable.h"
#include "lpc/include/function.h"
#include "efuns/ed.h"

static int inherits (program_t *, program_t *);

#ifdef F_ALLOCATE
void
f_allocate (void)
{
  sp->u.arr = allocate_array (sp->u.number);
  sp->type = T_ARRAY;
}
#endif


#ifdef F_ALLOCATE_BUFFER
void
f_allocate_buffer (void)
{
  buffer_t *buf;

  buf = allocate_buffer (sp->u.number);
  if (buf)
    {
      pop_stack ();
      push_refed_buffer (buf);
    }
  else
    {
      assign_svalue (sp, &const0);
    }
}
#endif


#ifdef F_CHILDREN
void
f_children (void)
{
  array_t *vec;

  vec = children (sp->u.string);
  free_string_svalue (sp);
  put_array (vec);
}
#endif


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


#ifdef F_CLEAR_BIT
void
f_clear_bit (void)
{
  char *str;
  int len, ind, bit;

  if (sp->u.number > CONFIG_INT (__MAX_BITFIELD_BITS__))
    error ("clear_bit() bit requested : %d > maximum bits: %d\n",
           sp->u.number, CONFIG_INT (__MAX_BITFIELD_BITS__));
  bit = (sp--)->u.number;
  if (bit < 0)
    error ("Bad argument 2 (negative) to clear_bit().\n");
  ind = bit / 6;
  bit %= 6;
  len = SVALUE_STRLEN (sp);
  if (ind >= len)
    return;			/* return first arg unmodified */
  unlink_string_svalue (sp);
  str = sp->u.string;

  if (str[ind] > 0x3f + ' ' || str[ind] < ' ')
    error ("Illegal bit pattern in clear_bit character %d\n", ind);
  str[ind] = ((str[ind] - ' ') & ~(1 << bit)) + ' ';
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


#ifdef F_CLONE_OBJECT
void
f_clone_object (void)
{
  svalue_t *arg = sp - st_num_arg + 1;

  object_t *ob = clone_object (arg->u.string, st_num_arg - 1);
  free_string_svalue (sp);
  if (ob)
    {
      put_unrefed_undested_object (ob, "f_clone_object");
    }
  else
    *sp = const0;
}
#endif


#ifdef F_DEEP_INHERIT_LIST
void
f_deep_inherit_list (void)
{
  array_t *vec;

  vec = deep_inherit_list (sp->u.ob);

  free_object (sp->u.ob, "f_deep_inherit_list");
  put_array (vec);
}
#endif


#ifdef F_DESTRUCT
void
f_destruct (void)
{
  destruct_object (sp->u.ob);
  sp--;				/* Ok since the object was removed from the stack */
}
#endif


#ifdef F_ED
void
f_ed (void)
{
  if (!command_giver || !command_giver->interactive)
    {
      pop_n_elems (st_num_arg);
      return;
    }

  if (!st_num_arg)
    {
      /* ed() */
      ed_start (0, 0, 0, 0, 0);
    }
  else if (st_num_arg == 1)
    {
      /* ed(fname) */
      ed_start (sp->u.string, 0, 0, 0, 0);
      pop_stack ();
    }
  else if (st_num_arg == 2)
    {
      /* ed(fname,exitfn) */
      ed_start ((sp - 1)->u.string, 0, sp->u.string, 0, current_object);
      pop_2_elems ();
    }
  else if (st_num_arg == 3)
    {
      /* ed(fname,exitfn,restricted) / ed(fname,writefn,exitfn) */
      if (sp->type == T_NUMBER)
        {
          ed_start ((sp - 2)->u.string, 0, (sp - 1)->u.string, sp->u.number,
                    current_object);
        }
      else if (sp->type == T_STRING)
        {
          ed_start ((sp - 2)->u.string, (sp - 1)->u.string, sp->u.string, 0,
                    current_object);
        }
      else
        {
          bad_argument (sp, T_NUMBER | T_STRING, 3, F_ED);
        }
      pop_3_elems ();
    }
  else
    {				/* st_num_arg == 4 */
      /* ed(fname,writefn,exitfn,restricted) */
      if (!((sp - 1)->type == T_STRING))
        bad_argument (sp - 1, T_STRING, 3, F_ED);
      if (!(sp->type == T_NUMBER))
        bad_argument (sp, T_NUMBER, 4, F_ED);
      ed_start ((sp - 3)->u.string, (sp - 2)->u.string, (sp - 1)->u.string,
                sp->u.number, current_object);
      pop_n_elems (4);
    }
}
#endif


#ifdef F_ED_CMD
void
f_ed_cmd (void)
{
  char *res;

  if (current_object->flags & O_DESTRUCTED)
    error ("destructed objects can't use ed.\n");

  if (!(current_object->flags & O_IN_EDIT))
    error ("ed_cmd() called with no ed session active.\n");

  res = object_ed_cmd (current_object, sp->u.string);

  free_string_svalue (sp);
  if (res)
    {
      sp->subtype = STRING_MALLOC;
      sp->u.string = res;
    }
  else
    {
      sp->subtype = STRING_CONSTANT;
      sp->u.string = "";
    }
}
#endif


#ifdef F_ED_START
void
f_ed_start (void)
{
  char *res;
  char *fname;
  int restr = 0;

  if (st_num_arg == 2)
    restr = (sp--)->u.number;

  if (st_num_arg)
    fname = sp->u.string;
  else
    fname = 0;

  if (current_object->flags & O_DESTRUCTED)
    error ("destructed objects can't use ed.\n");

  if (current_object->flags & O_IN_EDIT)
    error ("ed_start() called while an ed session is already started.\n");

  res = object_ed_start (current_object, fname, restr);

  if (fname)
    free_string_svalue (sp);
  else
    ++sp;

  if (res)
    {
      sp->subtype = STRING_MALLOC;
      sp->u.string = res;
    }
  else
    {
      sp->subtype = STRING_CONSTANT;
      sp->u.string = "";
    }
}
#endif


#ifdef F_FILE_NAME
void
f_file_name (void)
{
  char *res;

  /* This function now returns a leading '/' */
  res = (char *) add_slash (sp->u.ob->name);
  free_object (sp->u.ob, "f_file_name");
  put_malloced_string (res);
}
#endif


#ifdef F_FILTER
void
f_filter (void)
{
  svalue_t *arg = sp - st_num_arg + 1;

  if (arg->type == T_MAPPING)
    filter_mapping (arg, st_num_arg);
  else
    filter_array (arg, st_num_arg);
}
#endif


#ifdef F_FIND_OBJECT
void
f_find_object (void)
{
  object_t *ob;
  if ((sp--)->u.number)
    ob = find_object (sp->u.string);
  else
    ob = find_object2 (sp->u.string);
  free_string_svalue (sp);
  if (ob)
    {
      object_t *old_ob = ob;
      /* object_visible might change ob, a global - Sym */
      if (object_visible (ob))
        {
          /* find_object only returns undested objects */
          put_unrefed_undested_object (old_ob, "find_object");
        }
      else
        *sp = const0;
    }
  else
    *sp = const0;
}
#endif


#ifdef F_FUNCTION_EXISTS
void
f_function_exists (void)
{
  char *str, *res;
  int l;
  object_t *ob;
  int flag = 0;

  if (st_num_arg > 1)
    {
      if (st_num_arg > 2)
        flag = (sp--)->u.number;
      ob = (sp--)->u.ob;
      free_object (ob, "f_function_exists");
    }
  else
    {
      if (current_object->flags & O_DESTRUCTED)
        {
          free_string_svalue (sp);
          *sp = const0;
          return;
        }
      ob = current_object;
    }

  str = function_exists (sp->u.string, ob, flag);
  free_string_svalue (sp);
  if (str)
    {
      l = SHARED_STRLEN (str) - 2;	/* no .c */
      res = new_string (l + 1, "function_exists");
      res[0] = '/';
      strncpy (res + 1, str, l);
      res[l + 1] = 0;

      sp->subtype = STRING_MALLOC;
      sp->u.string = res;
    }
  else
    *sp = const0;
}
#endif


#ifdef F_GET_CONFIG
void
f_get_config (void)
{
  if (!get_config_item (sp, sp))
    error ("Bad argument to get_config()\n");
}
#endif



#ifdef F_IN_EDIT
void
f_in_edit (void)
{
  char *fn;
  ed_buffer_t *eb = 0;

#ifdef OLD_ED
  if (sp->u.ob->interactive)
    eb = sp->u.ob->interactive->ed_buffer;
#else
  if (sp->u.ob->flags & O_IN_EDIT)
    eb = find_ed_buffer (sp->u.ob);
#endif
  if (eb && (fn = eb->fname))
    {
      free_object (sp->u.ob, "f_in_edit:1");
      put_constant_string (fn);	/* is this safe?  - Beek */
      return;
    }
  free_object (sp->u.ob, "f_in_edit:1");
  *sp = const0;
  return;
}
#endif


#ifdef F_INHERITS
static int inherits (program_t * prog, program_t * thep) {
  int j, k = prog->num_inherited;
  program_t *pg;

  for (j = 0; j < k; j++)
    {
      if ((pg = prog->inherit[j].prog) == thep)
        return 1;
      if (!strcmp (pg->name, thep->name))
        return 2;
      if (inherits (pg, thep))
        return 1;
    }
  return 0;
}

void
f_inherits (void)
{
  object_t *ob, *base;
  int i;

  base = (sp--)->u.ob;
  ob = find_object2 (sp->u.string);
  if (!ob)
    {
      free_object (base, "f_inherits");
      assign_svalue (sp, &const0);
      return;
    }
  i = inherits (base->prog, ob->prog);
  free_object (base, "f_inherits");
  free_string_svalue (sp);
  put_number (i);
}
#endif


#ifdef F_SHALLOW_INHERIT_LIST
void
f_shallow_inherit_list (void)
{
  array_t *vec;

  vec = inherit_list (sp->u.ob);

  free_object (sp->u.ob, "f_inherit_list");
  put_array (vec);
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


#ifdef F_MASTER
void
f_master (void)
{
  if (!master_ob)
    push_number (0);
  else
    push_object (master_ob);
}
#endif



#ifdef F_MEMBER_ARRAY
void
f_member_array (void)
{
  array_t *v;
  int i;

  if (st_num_arg > 2)
    {
      i = (sp--)->u.number;
      if (i < 0)
        bad_arg (3, F_MEMBER_ARRAY);
    }
  else
    i = 0;

  if (sp->type == T_STRING)
    {
      char *res;
      CHECK_TYPES (sp - 1, T_NUMBER, 1, F_MEMBER_ARRAY);
      if (i > (int)SVALUE_STRLEN (sp))
        error
          ("Index to start search from in member_array() is > string length.\n");
      if ((res = strchr (sp->u.string + i, (sp - 1)->u.number)))
        i = res - sp->u.string;
      else
        i = -1;
      free_string_svalue (sp--);
    }
  else
    {
      int size = (v = sp->u.arr)->size;
      svalue_t *sv;
      svalue_t *find;
      int flen = 0;

      find = (sp - 1);
      /* optimize a bit */
      if (find->type == T_STRING)
        {
          /* *not* COUNTED_STRLEN() which can do a (costly) strlen() call */
          if (find->subtype & STRING_COUNTED)
            flen = MSTR_SIZE (find->u.string);
          else
            flen = 0;
        }

      for (; i < size; i++)
        {
          switch (find->type | (sv = v->item + i)->type)
            {
            case T_STRING:
              if (flen && (sv->subtype & STRING_COUNTED)
                  && flen != MSTR_SIZE (sv->u.string))
                continue;
              if (strcmp (find->u.string, sv->u.string))
                continue;
              break;
            case T_NUMBER:
              if (find->u.number == sv->u.number)
                break;
              continue;
            case T_REAL:
              if (find->u.real == sv->u.real)
                break;
              continue;
            case T_ARRAY:
              if (find->u.arr == sv->u.arr)
                break;
              continue;
            case T_OBJECT:
              {
                if (sv->u.ob->flags & O_DESTRUCTED)
                  {
                    assign_svalue (sv, &const0);
                    continue;
                  }
                if (find->u.ob == sv->u.ob)
                  break;
                continue;
              }
            case T_MAPPING:
              if (find->u.map == sv->u.map)
                break;
              continue;
            case T_FUNCTION:
              if (find->u.fp == sv->u.fp)
                break;
              continue;
            case T_BUFFER:
              if (find->u.buf == sv->u.buf)
                break;
              continue;
            default:
              if (sv->type == T_OBJECT && sv->u.ob->flags & O_DESTRUCTED)
                {
                  assign_svalue (sv, &const0);
                  if (find->type == T_NUMBER && !find->u.number)
                    break;
                }
              continue;
            }
          break;
        }
      if (i == size)
        i = -1;			/* Return -1 for failure */
      free_array (v);
      free_svalue (find, "f_member_array");
      sp--;
    }
  put_number (i);
}
#endif


#ifdef F_MESSAGE
void
f_message (void)
{
  array_t *use = NULL, *avoid;
  int num_arg = st_num_arg;
  svalue_t *args;

  args = sp - num_arg + 1;
  switch (args[2].type)
    {
    case T_OBJECT:
    case T_STRING:
      use = allocate_empty_array (1);
      use->item[0] = args[2];
      args[2].type = T_ARRAY;
      args[2].u.arr = use;
      break;
    case T_ARRAY:
      use = args[2].u.arr;
      break;
    default:
      bad_argument (&args[2], T_OBJECT | T_STRING | T_ARRAY, 3, F_MESSAGE);
    }

  if (num_arg == 4)
    {
      switch (args[3].type)
        {
        case T_OBJECT:
          avoid = allocate_empty_array (1);
          avoid->item[0] = args[3];
          args[3].type = T_ARRAY;
          args[3].u.arr = avoid;
          break;
        case T_ARRAY:
          avoid = args[3].u.arr;
          break;
        default:
          avoid = &the_null_array;
        }
    }
  else
    avoid = &the_null_array;

  if (use != NULL)
    do_message (&args[0], &args[1], use, avoid, 1);
  pop_n_elems (num_arg);
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


#ifdef F_PREVIOUS_OBJECT
void
f_previous_object (void)
{
  object_t *ob;
  control_stack_t *p;
  int i;

  if ((i = sp->u.number) > 0)
    {
      if (i >= CONFIG_INT (__MAX_CALL_DEPTH__))
        {
          sp->u.number = 0;
          return;
        }
      ob = 0;
      p = csp;
      do
        {
          if ((p->framekind & FRAME_OB_CHANGE) && !(--i))
            {
              ob = p->prev_ob;
              break;
            }
        }
      while (--p >= control_stack);
    }
  else if (i == -1)
    {
      array_t *v;

      i = previous_ob ? 1 : 0;
      p = csp;
      do
        {
          if ((p->framekind & FRAME_OB_CHANGE) && p->prev_ob)
            i++;
        }
      while (--p >= control_stack);
      v = allocate_empty_array (i);
      p = csp;
      if (previous_ob)
        {
          if (!(previous_ob->flags & O_DESTRUCTED))
            {
              v->item[0].type = T_OBJECT;
              v->item[0].u.ob = previous_ob;
              add_ref (previous_ob, "previous_object(-1)");
            }
          else
            v->item[0] = const0;
          i = 1;
        }
      else
        i = 0;
      do
        {
          if ((p->framekind & FRAME_OB_CHANGE) && (ob = p->prev_ob))
            {
              if (!(ob->flags & O_DESTRUCTED))
                {
                  v->item[i].type = T_OBJECT;
                  v->item[i].u.ob = ob;
                  add_ref (ob, "previous_object(-1)");
                }
              else
                v->item[i] = const0;
              i++;
            }
        }
      while (--p >= control_stack);
      put_array (v);
      return;
    }
  else if (i < 0)
    {
      error ("Illegal negative argument to previous_object()\n");
    }
  else
    ob = previous_ob;
  if (!ob || (ob->flags & O_DESTRUCTED))
    sp->u.number = 0;
  else
    {
      put_unrefed_undested_object (ob, "previous_object()");
    }
}
#endif


#ifdef F_QUERY_ED_MODE
void
f_query_ed_mode (void)
{
  /* n = prompt for line 'n'
     0 = normal ed prompt
     -1 = not in ed
     -2 = more prompt */
  if (current_object->flags & O_IN_EDIT)
    {
      push_number (object_ed_mode (current_object));
    }
  else
    push_number (-1);
}
#endif


#ifdef F_QUERY_LOAD_AVERAGE
void
f_query_load_average (void)
{
  copy_and_push_string (query_load_av ());
}
#endif


#ifdef F_QUERY_PRIVS
void
f_query_privs (void)
{
  ob = sp->u.ob;
  if (ob->privs != NULL)
    {
      free_object (ob, "f_query_privs");
      sp->type = T_STRING;
      sp->u.string = make_shared_string (ob->privs);
      sp->subtype = STRING_SHARED;
    }
  else
    {
      free_object (ob, "f_query_privs");
      *sp = const0;
    }
}
#endif


#ifdef F_RANDOM
void
f_random (void)
{
  if (sp->u.number <= 0)
    {
      sp->u.number = 0;
      return;
    }
  sp->u.number = rand () % sp->u.number;
}
#endif


#ifdef F_RESTORE_VARIABLE
void
f_restore_variable (void)
{
  svalue_t v;

  unlink_string_svalue (sp);
  v.type = T_NUMBER;

  restore_variable (&v, sp->u.string);
  FREE_MSTR (sp->u.string);
  *sp = v;
}
#endif


#ifdef F_SAVE_VARIABLE
void
f_save_variable (void)
{
  char *p;

  p = save_variable (sp);
  pop_stack ();
  push_malloced_string (p);
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


#ifdef F_SET_EVAL_LIMIT
/* warning: do not enable this without using valid_override() in the master
   object and a set_eval_limit() simul_efun to restrict access.
*/
void
f_set_eval_limit (void)
{
  switch (sp->u.number)
    {
    case 0:
      sp->u.number = eval_cost = CONFIG_INT (__MAX_EVAL_COST__);
      break;
    case -1:
      sp->u.number = eval_cost;
      break;
    case 1:
      sp->u.number = CONFIG_INT (__MAX_EVAL_COST__);
      break;
    default:
      CONFIG_INT (__MAX_EVAL_COST__) = sp->u.number;
      break;
    }
}
#endif


#ifdef F_SET_BIT
void
f_set_bit (void)
{
  char *str;
  int len, old_len, ind, bit;

  if (sp->u.number > CONFIG_INT (__MAX_BITFIELD_BITS__))
    error ("set_bit() bit requested: %d > maximum bits: %d\n", sp->u.number,
           CONFIG_INT (__MAX_BITFIELD_BITS__));
  bit = (sp--)->u.number;
  if (bit < 0)
    error ("Bad argument 2 (negative) to set_bit().\n");
  ind = bit / 6;
  bit %= 6;
  old_len = len = SVALUE_STRLEN (sp);
  if (ind >= len)
    len = ind + 1;
  if (ind < old_len)
    {
      unlink_string_svalue (sp);
      str = sp->u.string;
    }
  else
    {
      str = new_string (len, "f_set_bit: str");
      str[len] = '\0';
      if (old_len)
        memcpy (str, sp->u.string, old_len);
      if (len > old_len)
        memset (str + old_len, ' ', len - old_len);
      free_string_svalue (sp);
      sp->subtype = STRING_MALLOC;
      sp->u.string = str;
    }

  if (str[ind] > 0x3f + ' ' || str[ind] < ' ')
    error ("Illegal bit pattern in set_bit character %d\n", ind);
  str[ind] = ((str[ind] - ' ') | (1 << bit)) + ' ';
}
#endif


#ifdef F_SET_HIDE
void
f_set_hide (void)
{
  if (!valid_hide (current_object))
    {
      sp--;
      return;
    }
  if ((sp--)->u.number)
    {
      if (!(current_object->flags & O_HIDDEN) && current_object->interactive)
        num_hidden++;
      current_object->flags |= O_HIDDEN;
    }
  else
    {
      if ((current_object->flags & O_HIDDEN) && current_object->interactive)
        num_hidden--;
      current_object->flags &= ~O_HIDDEN;
    }
}
#endif


#ifdef F_SET_LIGHT
void
f_set_light (void)
{
  object_t *o1;

  add_light (current_object, sp->u.number);
  o1 = current_object;
#ifndef NO_ENVIRONMENT
  while (o1->super)
    o1 = o1->super;
#endif
  sp->u.number = o1->total_light;
}
#endif


#ifdef F_SET_PRIVS
void
f_set_privs (void)
{
  object_t *ob;

  ob = (sp - 1)->u.ob;
  if (ob->privs != NULL)
    free_string (ob->privs);
  if (!(sp->type == T_STRING))
    {
      ob->privs = NULL;
      sp--;			/* It's a number */
    }
  else
    {
      ob->privs = make_shared_string (sp->u.string);
      free_string_svalue (sp--);
    }
  free_object (ob, "f_set_privs");
  sp--;
}
#endif


#ifdef F_SHADOW
void
f_shadow (void)
{
  object_t *ob;

  ob = (sp - 1)->u.ob;
  if (!((sp--)->u.number))
    {
      ob = ob->shadowed;
      free_object (sp->u.ob, "f_shadow:1");
      if (ob)
        {
          add_ref (ob, "shadow(ob, 0)");
          sp->u.ob = ob;
        }
      else
        *sp = const0;
      return;
    }
  if (ob == current_object)
    {
      error ("shadow: Can't shadow self\n");
    }
  if (validate_shadowing (ob))
    {
      if (current_object->flags & O_DESTRUCTED)
        {
          free_object (ob, "f_shadow:2");
          *sp = const0;
          return;
        }
      /*
       * The shadow is entered first in the chain.
       */
      while (ob->shadowed)
        ob = ob->shadowed;
      current_object->shadowing = ob;
      ob->shadowed = current_object;
      free_object (sp->u.ob, "f_shadow:3");
      add_ref (ob, "shadow(ob, 1)");
      sp->u.ob = ob;
      return;
    }
  free_object (sp->u.ob, "f_shadow:4");
  *sp = const0;
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


#ifdef F_SHUTDOWN
void
f_shutdown (void)
{
  do_shutdown (st_num_arg ? sp->u.number : (*++sp = const0, 0));
}
#endif


#ifdef F_SIZEOF
void
f_sizeof (void)
{
  int i;

  switch (sp->type)
    {
    case T_CLASS:
      i = sp->u.arr->size;
      free_class (sp->u.arr);
      break;
    case T_ARRAY:
      i = sp->u.arr->size;
      free_array (sp->u.arr);
      break;
    case T_MAPPING:
      i = sp->u.map->count;
      free_mapping (sp->u.map);
      break;
    case T_BUFFER:
      i = sp->u.buf->size;
      free_buffer (sp->u.buf);
      break;
    case T_STRING:
      i = SVALUE_STRLEN (sp);
      free_string_svalue (sp);
      break;
    default:
      i = 0;
      free_svalue (sp, "f_sizeof");
    }
  sp->type = T_NUMBER;
  sp->u.number = i;
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


#ifdef F_TELL_OBJECT
void
f_tell_object (void)
{
  tell_object ((sp - 1)->u.ob, sp->u.string);
  free_string_svalue (sp--);
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
      ob = find_object (arg[0].u.string);
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


#ifdef F_TEST_BIT
void
f_test_bit (void)
{
  int ind = (sp--)->u.number;

  if (ind / 6 >= (int)SVALUE_STRLEN (sp))
    {
      free_string_svalue (sp);
      *sp = const0;
      return;
    }
  if (ind < 0)
    error ("Bad argument 2 (negative) to test_bit().\n");
  if ((sp->u.string[ind / 6] - ' ') & (1 << (ind % 6)))
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


#ifdef F_NEXT_BIT
void
f_next_bit (void)
{
  int start = (sp--)->u.number;
  int len = SVALUE_STRLEN (sp);
  int which, bit = 0, value;

  if (!len || start / 6 >= len)
    {
      free_string_svalue (sp);
      put_number (-1);
      return;
    }
  /* Find the next bit AFTER start */
  if (start > 0)
    {
      if (start % 6 == 5)
        {
          which = (start / 6) + 1;
          value = sp->u.string[which] - ' ';
        }
      else
        {
          /* we have a partial byte to check */
          which = start / 6;
          bit = 0x3f - ((1 << ((start % 6) + 1)) - 1);
          value = (sp->u.string[which] - ' ') & bit;
        }
    }
  else
    {
      which = 0;
      value = *sp->u.string - ' ';
    }

  while (1)
    {
      if (value)
        {
          if (value & 0x07)
            {
              if (value & 0x01)
                bit = which * 6;
              else if (value & 0x02)
                bit = which * 6 + 1;
              else if (value & 0x04)
                bit = which * 6 + 2;
              break;
            }
          else if (value & 0x38)
            {
              if (value & 0x08)
                bit = which * 6 + 3;
              else if (value & 0x10)
                bit = which * 6 + 4;
              else if (value & 0x20)
                bit = which * 6 + 5;
              break;
            }
        }
      which++;
      if (which == len)
        {
          bit = -1;
          break;
        }
      value = sp->u.string[which] - ' ';
    }

  free_string_svalue (sp);
  put_number (bit);
}
#endif


#ifdef F_THIS_OBJECT
void
f_this_object (void)
{
  if (current_object->flags & O_DESTRUCTED)	/* Fixed from 3.1.1 */
    *++sp = const0;
  else
    push_object (current_object);
}
#endif


#ifdef F_THIS_PLAYER
void
f_this_player (void)
{
  if (sp->u.number)
    {
      if (current_interactive)
        put_unrefed_object (current_interactive, "this_player(1)");
      else
        sp->u.number = 0;
    }
  else
    {
      if (command_giver)
        put_unrefed_object (command_giver, "this_player(0)");
      /* else zero is on stack already */
    }
}
#endif


#ifdef F_SET_THIS_PLAYER
void
f_set_this_player (void)
{
  if (sp->type == T_NUMBER)
    command_giver = 0;
  else
    command_giver = sp->u.ob;
  pop_stack ();
}
#endif


#ifdef F_TO_FLOAT
void
f_to_float (void)
{
  double temp = 0;

  switch (sp->type)
    {
    case T_NUMBER:
      sp->type = T_REAL;
      sp->u.real = (double) sp->u.number;
      break;
    case T_STRING:
      sscanf (sp->u.string, "%lf", &temp);
      free_string_svalue (sp);
      sp->type = T_REAL;
      sp->u.real = temp;
    }
}
#endif


#ifdef F_TO_INT
void
f_to_int (void)
{
  switch (sp->type)
    {
    case T_REAL:
      sp->type = T_NUMBER;
      sp->u.number = (int) sp->u.real;
      break;
    case T_STRING:
      {
        int temp;

        temp = atoi (sp->u.string);
        free_string_svalue (sp);
        sp->u.number = temp;
        sp->type = T_NUMBER;
        break;
      }
    case T_BUFFER:
      if (sp->u.buf->size < sizeof (int))
        {
          free_buffer (sp->u.buf);
          *sp = const0;
        }
      else
        {
          int hostint, netint;

          memcpy ((char *) &netint, sp->u.buf->item, sizeof (int));
          hostint = ntohl (netint);
          free_buffer (sp->u.buf);
          put_number (hostint);
        }
    }
}
#endif


#ifdef F_TYPEOF
void
f_typeof (void)
{
  char *t = type_name (sp->type);

  free_svalue (sp, "f_typeof");
  put_constant_string (t);
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


#ifdef F_WRITE
void
f_write (void)
{
  do_write (sp);
  pop_stack ();
}
#endif


#ifdef F_RELOAD_OBJECT
void
f_reload_object (void)
{
  reload_object (sp->u.ob);
  free_object ((sp--)->u.ob, "f_reload_object");
}
#endif


#ifdef F_QUERY_SHADOWING
void
f_query_shadowing (void)
{
  if ((sp->type == T_OBJECT) && (ob = sp->u.ob)->shadowing)
    {
      add_ref (ob->shadowing, "query_shadowing(ob)");
      sp->u.ob = ob->shadowing;
      free_object (ob, "f_query_shadowing");
    }
  else
    {
      free_svalue (sp, "f_query_shadowing");
      *sp = const0;
    }
}
#endif


#ifdef F_SET_RESET
void
f_set_reset (void)
{
  if (st_num_arg == 2)
    {
      (sp - 1)->u.ob->next_reset = current_time + sp->u.number;
      free_object ((--sp)->u.ob, "f_set_reset:1");
      sp--;
    }
  else if (CONFIG_INT (__TIME_TO_RESET__) > 0)
    {
      sp->u.ob->next_reset = current_time + CONFIG_INT (__TIME_TO_RESET__) / 2
        + rand () % (CONFIG_INT (__TIME_TO_RESET__) / 2);
      free_object ((sp--)->u.ob, "f_set_reset:2");
    }
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


#ifdef F_FLUSH_MESSAGES
void
f_flush_messages (void)
{
  if (st_num_arg == 1)
    {
      if (sp->u.ob->interactive)
        flush_message (sp->u.ob->interactive);
      pop_stack ();
    }
  else
    {
      int i;

      for (i = 0; i < max_users; i++)
        {
          if (all_users[i] && !(all_users[i]->iflags & CLOSING))
            flush_message (all_users[i]);
        }
    }
}
#endif


/* I forgot who wrote this, please claim it :) */
#ifdef F_REMOVE_SHADOW
void
f_remove_shadow (void)
{
  object_t *ob;

  ob = current_object;
  if (st_num_arg)
    {
      ob = sp->u.ob;
      pop_stack ();
    }
  if (!ob || !ob->shadowing)
    push_number (0);
  else
    {
      if (ob->shadowed)
        ob->shadowed->shadowing = ob->shadowing;
      if (ob->shadowing)
        ob->shadowing->shadowed = ob->shadowed;
      ob->shadowing = ob->shadowed = 0;
      push_number (1);
    }
}
#endif


/* Beek again */
#ifdef F_STORE_VARIABLE
void
f_store_variable (void)
{
  int idx;
  svalue_t *sv;
  unsigned short type;

  idx = find_global_variable (current_object->prog, (sp - 1)->u.string, &type);
  if (idx == -1)
    error ("No variable named '%s'!\n", (sp - 1)->u.string);
  sv = &current_object->variables[idx];
  free_svalue (sv, "f_store_variable");
  *sv = *sp--;
  free_string_svalue (sp--);
}
#endif


#ifdef F_FETCH_VARIABLE
void
f_fetch_variable (void)
{
  int idx;
  svalue_t *sv;
  unsigned short type;

  idx = find_global_variable (current_object->prog, sp->u.string, &type);
  if (idx == -1)
    error ("No variable named '%s'!\n", sp->u.string);
  sv = &current_object->variables[idx];
  free_string_svalue (sp--);
  push_svalue (sv);
}
#endif


/* Beek */
#ifdef F_SET_PROMPT
void
f_set_prompt (void)
{
  object_t *who;
  if (st_num_arg == 2)
    {
      who = sp->u.ob;
      pop_stack ();
    }
  else
    who = command_giver;

  if (!who || who->flags & O_DESTRUCTED || !who->interactive)
    error ("Prompts can only be set for interactives.\n");

  /* Future work */
  /* ed() will nuke this; also we have to make sure the string will get
   * freed */
}
#endif


/* Gudu@VR wrote copy_array() and copy_mapping() which this is heavily
 * based on.  I made it into a general copy() efun which incorporates
 * both. -Beek
 */
#ifdef F_COPY
static int depth;

static void deep_copy_svalue (svalue_t *, svalue_t *);

static array_t *
deep_copy_array (array_t * arg)
{
  array_t *vec;
  int i;

  vec = allocate_empty_array (arg->size);
  for (i = 0; i < arg->size; i++)
    deep_copy_svalue (&arg->item[i], &vec->item[i]);
  return vec;
}

static int
doCopy (mapping_t * map, mapping_node_t * elt, mapping_t * dest)
{
  svalue_t *sp;
  (void) map; /* unused */

  sp = find_for_insert (dest, &elt->values[0], 1);
  if (!sp)
    {
      mapping_too_large ();
      return 1;
    }

  deep_copy_svalue (&elt->values[1], sp);
  return 0;
}

static mapping_t *
deep_copy_mapping (mapping_t * arg)
{
  mapping_t *map;

  map = allocate_mapping (0);	/* this should be fixed.  -Beek */
  mapTraverse (arg, (int (*)()) doCopy, map);
  return map;
}

static void
deep_copy_svalue (svalue_t * from, svalue_t * to)
{
  switch (from->type)
    {
    case T_ARRAY:
    case T_CLASS:
      depth++;
      if (depth > MAX_SAVE_SVALUE_DEPTH)
        {
          depth = 0;
          error
            ("Mappings, arrays and/or classes nested too deep (%d) for copy()\n",
             MAX_SAVE_SVALUE_DEPTH);
        }
      *to = *from;
      to->u.arr = deep_copy_array (from->u.arr);
      depth--;
      break;
    case T_MAPPING:
      depth++;
      if (depth > MAX_SAVE_SVALUE_DEPTH)
        {
          depth = 0;
          error
            ("Mappings, arrays and/or classes nested too deep (%d) for copy()\n",
             MAX_SAVE_SVALUE_DEPTH);
        }
      *to = *from;
      to->u.map = deep_copy_mapping (from->u.map);
      depth--;
      break;
    default:
      assign_svalue_no_free (to, from);
    }
}

void
f_copy (void)
{
  svalue_t ret;

  depth = 0;
  deep_copy_svalue (sp, &ret);
  free_svalue (sp, "f_copy");
  *sp = ret;
}
#endif


/* Gudu@VR */
/* flag and extra info by Beek */
#ifdef F_FUNCTIONS
void
f_functions (void)
{
  int i, j, num, index;
  array_t *vec, *subvec;
  runtime_function_u *func_entry;
  compiler_function_t *funp;
  program_t *prog;
  int flag = (sp--)->u.number;
  unsigned short *types;
  char buf[256];
  char *end = EndOf (buf);
  program_t *progp;

  progp = sp->u.ob->prog;
  num = progp->num_functions_total;
  if (num && progp->function_table[progp->num_functions_defined - 1].name[0]
      == APPLY___INIT_SPECIAL_CHAR)
    num--;

  vec = allocate_empty_array (num);
  i = num;

  while (i--)
    {
      prog = sp->u.ob->prog;
      index = i;
      func_entry = FIND_FUNC_ENTRY (prog, index);

      /* Walk up the inheritance tree to the real definition */
      while (prog->function_flags[index] & NAME_INHERITED)
        {
          prog = prog->inherit[func_entry->inh.offset].prog;
          index = func_entry->inh.function_index_offset;
          func_entry = FIND_FUNC_ENTRY (prog, index);
        }

      funp = prog->function_table + func_entry->def.f_index;

      if (flag)
        {
          if (prog->type_start && prog->type_start[index] != INDEX_START_NONE)
            types = &prog->argument_types[prog->type_start[index]];
          else
            types = 0;

          vec->item[i].type = T_ARRAY;
          subvec = vec->item[i].u.arr =
            allocate_empty_array (3 + func_entry->def.num_arg);

          subvec->item[0].type = T_STRING;
          subvec->item[0].subtype = STRING_SHARED;
          subvec->item[0].u.string = ref_string (funp->name);

          subvec->item[1].type = T_NUMBER;
          subvec->item[1].subtype = 0;
          subvec->item[1].u.number = func_entry->def.num_arg;

          get_type_name (buf, end, funp->type);
          subvec->item[2].type = T_STRING;
          subvec->item[2].subtype = STRING_SHARED;
          subvec->item[2].u.string = make_shared_string (buf);

          for (j = 0; j < func_entry->def.num_arg; j++)
            {
              if (types)
                {
                  get_type_name (buf, end, types[j]);
                  subvec->item[3 + j].type = T_STRING;
                  subvec->item[3 + j].subtype = STRING_SHARED;
                  subvec->item[3 + j].u.string = make_shared_string (buf);
                }
              else
                {
                  subvec->item[3 + j].type = T_NUMBER;
                  subvec->item[3 + j].u.number = 0;
                }
            }
        }
      else
        {
          vec->item[i].type = T_STRING;
          vec->item[i].subtype = STRING_SHARED;
          vec->item[i].u.string = ref_string (funp->name);
        }
    }

  pop_stack ();
  push_refed_array (vec);
}
#endif


/* Beek */
#ifdef F_VARIABLES
static void
fv_recurse (array_t * arr, int *idx, program_t * prog, int type, int flag)
{
  int i;
  array_t *subarr;
  char buf[256];
  char *end = EndOf (buf);

  for (i = 0; i < prog->num_inherited; i++)
    {
      fv_recurse (arr, idx, prog->inherit[i].prog,
                  type | prog->inherit[i].type_mod, flag);
    }
  for (i = 0; i < prog->num_variables_defined; i++)
    {
      if (flag)
        {
          arr->item[*idx + i].type = T_ARRAY;
          subarr = arr->item[*idx + i].u.arr = allocate_empty_array (2);
          subarr->item[0].type = T_STRING;
          subarr->item[0].subtype = STRING_SHARED;
          subarr->item[0].u.string = ref_string (prog->variable_table[i]);
          get_type_name (buf, end, prog->variable_types[i]);
          subarr->item[1].type = T_STRING;
          subarr->item[1].subtype = STRING_SHARED;
          subarr->item[1].u.string = make_shared_string (buf);
        }
      else
        {
          arr->item[*idx + i].type = T_STRING;
          arr->item[*idx + i].subtype = STRING_SHARED;
          arr->item[*idx + i].u.string = ref_string (prog->variable_table[i]);
        }
    }
  *idx += prog->num_variables_defined;
}

void
f_variables (void)
{
  int idx = 0;
  array_t *arr;
  int flag = (sp--)->u.number;
  program_t *prog = sp->u.ob->prog;

  arr = allocate_empty_array (prog->num_variables_total);
  fv_recurse (arr, &idx, prog, 0, flag);

  pop_stack ();
  push_refed_array (arr);
}
#endif


#ifdef F_FUNCTION_OWNER
void
f_function_owner (void)
{
  object_t *owner = sp->u.fp->hdr.owner;

  free_funp (sp->u.fp);
  put_unrefed_object (owner, "f_function_owner");
}
#endif


#ifdef F_RUSAGE
void
f_rusage (void)
{
  struct rusage rus;
  mapping_t *m;
  long usertime, stime;
  int maxrss;

  if (getrusage (RUSAGE_SELF, &rus) < 0)
    {
      m = allocate_mapping (0);
    }
  else
    {
      usertime = rus.ru_utime.tv_sec * 1000 + rus.ru_utime.tv_usec / 1000;
      stime = rus.ru_stime.tv_sec * 1000 + rus.ru_stime.tv_usec / 1000;
      maxrss = rus.ru_maxrss;
#ifdef sun
      maxrss *= getpagesize () / 1024;
#endif
      m = allocate_mapping (16);
      add_mapping_pair (m, "utime", usertime);
      add_mapping_pair (m, "stime", stime);
      add_mapping_pair (m, "maxrss", maxrss);
      add_mapping_pair (m, "ixrss", rus.ru_ixrss);
      add_mapping_pair (m, "idrss", rus.ru_idrss);
      add_mapping_pair (m, "isrss", rus.ru_isrss);
      add_mapping_pair (m, "minflt", rus.ru_minflt);
      add_mapping_pair (m, "majflt", rus.ru_majflt);
      add_mapping_pair (m, "nswap", rus.ru_nswap);
      add_mapping_pair (m, "inblock", rus.ru_inblock);
      add_mapping_pair (m, "oublock", rus.ru_oublock);
      add_mapping_pair (m, "msgsnd", rus.ru_msgsnd);
      add_mapping_pair (m, "msgrcv", rus.ru_msgrcv);
      add_mapping_pair (m, "nsignals", rus.ru_nsignals);
      add_mapping_pair (m, "nvcsw", rus.ru_nvcsw);
      add_mapping_pair (m, "nivcsw", rus.ru_nivcsw);
    }
  push_refed_mapping (m);
}
#endif
