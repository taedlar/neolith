#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "src/simulate.h"
#include "lpc/array.h"
#include "lpc/object.h"
#include "lpc/program.h"
#include "lpc/compiler.h"

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
          subarr = arr->item[*idx + i].u.arr = allocate_empty_array (2); /* e.g. ({ "weight", "int"}) */
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

