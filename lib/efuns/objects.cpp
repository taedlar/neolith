#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "rc/rc.h"
#include "lpc/array.h"
#include "lpc/object.h"
#include "lpc/otable.h"
#include "lpc/program.h"
#include "lpc/types.h"

#ifdef F_FILE_NAME
extern "C" void f_file_name (void) {
  char *res;

  /* This function now returns a leading '/' */
  res = (char *) add_slash (sp->u.ob->name);
  free_object (sp->u.ob, "f_file_name");
  put_malloced_string (res);
}
#endif


#ifdef F_DESTRUCT
extern "C" void f_destruct (void) {
  destruct_object (sp->u.ob);
  sp--;				/* Ok since the object was removed from the stack */
}
#endif


#ifdef F_CLONE_OBJECT
extern "C" void f_clone_object (void) {
  svalue_t *arg = sp - st_num_arg + 1;

  object_t *ob = clone_object (SVALUE_STRPTR(arg), st_num_arg - 1);
  free_string_svalue (sp);
  if (ob)
    {
      put_unrefed_undested_object (ob, "f_clone_object");
    }
  else
    *sp = const0;
}
#endif


#ifdef F_FIND_OBJECT
extern "C" void f_find_object (void) {
  object_t *ob;
  if ((sp--)->u.number)
    ob = find_or_load_object (SVALUE_STRPTR(sp));
  else
    ob = find_object_by_name (SVALUE_STRPTR(sp));
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


#ifdef F_RELOAD_OBJECT
extern "C" void f_reload_object (void) {
  reload_object (sp->u.ob);
  free_object ((sp--)->u.ob, "f_reload_object");
}
#endif


#ifdef F_CHILDREN
static array_t* children (const char *str) {

  int i, j;
  int t_sz;
  size_t sl, ol;
  object_t *ob;
  object_t **tmp_children;
  array_t *ret;
  int display_hidden;
  char tmpbuf[MAX_OBJECT_NAME_SIZE];

  display_hidden = -1;
  if (!strip_name (str, tmpbuf, sizeof tmpbuf))
    return &the_null_array;

  sl = strlen (tmpbuf);

  NEOLITH_HEAP_SCOPE (scope);
  if (!(tmp_children = (object_t **)
        DMALLOC (sizeof (object_t *) * (t_sz = 50),
                 TAG_TEMPORARY, "children: tmp_children")))
    return &the_null_array;	/* unable to malloc enough temp space */

  for (i = 0, ob = obj_list; ob; ob = ob->next_all)
    {
      ol = strlen (ob->name);
      if (((ol == sl) || ((ol > sl) && (ob->name[sl] == '#')))
          && !strncmp (tmpbuf, ob->name, sl))
        {
          if (ob->flags & O_HIDDEN)
            {
              if (display_hidden == -1)
                {
                  display_hidden = valid_hide (current_object);
                }
              if (!display_hidden)
                continue;
            }
          tmp_children[i] = ob;
          if ((++i == t_sz) && (!(tmp_children
                                  =
                                  RESIZE (tmp_children, (t_sz += 50),
                                          object_t *, TAG_TEMPORARY,
                                          "children: tmp_children: realloc"))))
            {
              /* unable to REALLOC more space 
               * (tmp_children is now NULL) */
              return &the_null_array;
            }
        }
    }
  if (i > CONFIG_INT (__MAX_ARRAY_SIZE__))
    {
      i = CONFIG_INT (__MAX_ARRAY_SIZE__);
    }
  ret = allocate_empty_array (i);
  for (j = 0; j < i; j++)
    {
      ret->item[j].type = T_OBJECT;
      ret->item[j].u.ob = tmp_children[j];
      add_ref (tmp_children[j], "children");
    }
  FREE ((void *) tmp_children);
  return ret;
}

extern "C" void f_children (void) {
  array_t *vec;

  vec = children (SVALUE_STRPTR(sp));
  free_string_svalue (sp);
  put_array (vec);
}
#endif


#ifdef F_OBJECTS
extern "C" void f_objects (void) {
  const char *func = NULL;
  object_t *ob, **tmp;
  array_t *ret;
  funptr_t *f = 0;
  int display_hidden = 0, t_sz, i, j, num_arg = st_num_arg;
  svalue_t *v;

  if (!num_arg)
    func = 0;
  else if (sp->type == T_FUNCTION)
    f = sp->u.fp;
  else
    func = SVALUE_STRPTR(sp);

  NEOLITH_HEAP_SCOPE (scope);
  if (!(tmp = (object_t **) new_string ((t_sz = 1000) * sizeof (object_t *),
                                        "TMP: objects: tmp")))
    fatal ("Out of memory!\n");

  push_malloced_string ((char *) tmp);

  for (i = 0, ob = obj_list; ob; ob = ob->next_all)
    {
      if (ob->flags & O_HIDDEN)
        {
          if (!display_hidden)
            display_hidden = 1 + !!valid_hide (current_object);
          if (!(display_hidden & 2))
            continue;
        }
      if (f)
        {
          push_object (ob);
          v = CALL_FUNCTION_POINTER_SLOT_CALL (f, 1);
          if (!v)
            {
              CALL_FUNCTION_POINTER_SLOT_FINISH();
              FREE_MSTR ((char *) tmp);
              sp--;
              free_svalue (sp, "f_objects");
              *sp = const0;
              return;
            }
          if (v->type == T_NUMBER && !v->u.number)
            {
              CALL_FUNCTION_POINTER_SLOT_FINISH();
              continue;
            }
          CALL_FUNCTION_POINTER_SLOT_FINISH();
        }
      else if (func)
        {
          push_object (ob);
          v = APPLY_SLOT_CALL (func, current_object, 1, ORIGIN_EFUN);
          if (!v)
            {
              APPLY_SLOT_FINISH_CALL();
              FREE_MSTR ((char *) tmp);
              sp--;
              free_svalue (sp, "f_objects");
              *sp = const0;
              return;
            }
          if ((v->type == T_NUMBER) && !v->u.number)
            {
              APPLY_SLOT_FINISH_CALL();
              continue;
            }
          APPLY_SLOT_FINISH_CALL();
        }

      tmp[i] = ob;
      if (++i == t_sz)
        {
          if (!
              (tmp =
               (object_t **) extend_string ((char *) tmp,
                                            (t_sz +=
                                             1000) * sizeof (object_t *))))
            fatal ("Out of memory!\n");
          else
            sp->u.malloc_string = (char *) tmp;
        }
    }
  if (i > CONFIG_INT (__MAX_ARRAY_SIZE__))
    i = CONFIG_INT (__MAX_ARRAY_SIZE__);
  ret = allocate_empty_array (i);
  for (j = 0; j < i; j++)
    {
      ret->item[j].type = T_OBJECT;
      ret->item[j].u.ob = tmp[j];
      add_ref (tmp[j], "objects");
    }

  FREE_MSTR ((char *) tmp);
  sp--;
  pop_n_elems (num_arg);
  (++sp)->type = T_ARRAY;
  sp->u.arr = ret;
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

extern "C" void f_inherits (void) {
  object_t *ob, *base;
  int i;

  base = (sp--)->u.ob;
  ob = find_object_by_name (SVALUE_STRPTR(sp));
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
/*
 * Returns a list of the immediate inherited files.
 *
 */
static array_t *
inherit_list (object_t * ob)
{
  array_t *ret;
  program_t *pr, *plist[256];
  int il, il2, next, cur;

  plist[0] = ob->prog;
  next = 1;
  cur = 0;

  pr = plist[cur];
  for (il2 = 0; il2 < (int) pr->num_inherited; il2++)
    {
      plist[next++] = pr->inherit[il2].prog;
    }

  next--;			/* don't count the file itself */
  ret = allocate_empty_array (next);

  for (il = 0; il < next; il++)
    {
      pr = plist[il + 1];
      SET_SVALUE_MALLOC_STRING (&ret->item[il], add_slash (pr->name));
    }
  return ret;
}

extern "C" void f_shallow_inherit_list (void) {
  array_t *vec;

  vec = inherit_list (sp->u.ob);

  free_object (sp->u.ob, "f_inherit_list");
  put_array (vec);
}
#endif


#ifdef F_DEEP_INHERIT_LIST
/*
 * Returns a list of all inherited files.
 *
 * Must be fixed so that any number of files can be returned, now max 256
 * (Sounds like a contradiction to me /Lars).
 */
static array_t *
deep_inherit_list (object_t * ob)
{
  array_t *ret;
  program_t *pr, *plist[256];
  int il, il2, next, cur;

  plist[0] = ob->prog;
  next = 1;
  cur = 0;

  for (; cur < next && next < 256; cur++)
    {
      pr = plist[cur];
      for (il2 = 0; il2 < (int) pr->num_inherited; il2++)
        plist[next++] = pr->inherit[il2].prog;
    }

  next--;
  ret = allocate_empty_array (next);

  for (il = 0; il < next; il++)
    {
      pr = plist[il + 1];
      SET_SVALUE_MALLOC_STRING (&ret->item[il], add_slash (pr->name));
    }
  return ret;
}

extern "C" void f_deep_inherit_list (void) {
  array_t *vec;

  vec = deep_inherit_list (sp->u.ob);

  free_object (sp->u.ob, "f_deep_inherit_list");
  put_array (vec);
}
#endif

