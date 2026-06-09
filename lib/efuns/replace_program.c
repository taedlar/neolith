#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "lpc/types.h"
#include "lpc/object.h"
#include "lpc/program.h"
#include "lpc/array.h"
#include "src/interpret.h"
#include "src/simul_efun.h"
#include "src/simulate.h"

/*
 * replace_program.c
 * replaces the program in a running object with one of the programs
 * it inherits, in order to save memory.
 * Ported from Amylaars LP 3.2 driver
 */

static program_t *search_inherited (char *, program_t *, int *);
static replace_ob_t *retrieve_replace_program_entry (void);

#ifdef F_REPLACE_PROGRAM
static program_t *
search_inherited (char *str, program_t * prg, int *offpnt)
{
  program_t *tmp;
  int i;

  for (i = 0; i < (int) prg->num_inherited; i++)
    {
      if (strcmp (str, prg->inherit[i].prog->name) == 0)
	{
	  *offpnt = prg->inherit[i].variable_index_offset;
	  return prg->inherit[i].prog;
	}
      else if ((tmp = search_inherited (str, prg->inherit[i].prog, offpnt)))
	{
	  *offpnt += prg->inherit[i].variable_index_offset;
	  return tmp;
	}
    }
  return (program_t *) 0;
}

static replace_ob_t *
retrieve_replace_program_entry ()
{
  replace_ob_t *r_ob;

  for (r_ob = obj_list_replace; r_ob; r_ob = r_ob->next)
    {
      if (r_ob->ob == current_object)
	{
	  return r_ob;
	}
    }
  return 0;
}

void
f_replace_program (int num_arg, int instruction)
{
  replace_ob_t *tmp;
  size_t name_len;
  char *name, *xname;
  program_t *new_prog;
  int var_offset;
  (void) num_arg; /* unused */

  if (sp->type != T_STRING)
    bad_arg (1, instruction);
  if (!current_object)
    error ("replace_program called with no current object\n");
  if (current_object == simul_efun_ob)
    error ("replace_program on simul_efun object\n");

  if (current_object->prog->func_ref)
    error ("cannot replace a program with function references.\n");

  name_len = strlen (SVALUE_STRPTR(sp));
  name = (char *) DMALLOC (name_len + 3, TAG_TEMPORARY, "replace_program");
  xname = name;
  strcpy (name, SVALUE_STRPTR(sp));
  if (name[name_len - 2] != '.' || name[name_len - 1] != 'c')
    strcat (name, ".c");
  if (*name == '/')
    name++;
  new_prog = search_inherited (name, current_object->prog, &var_offset);
  FREE (xname);
  if (!new_prog)
    {
      error ("program to replace the current with has to be inherited\n");
    }
  if (!(tmp = retrieve_replace_program_entry ()))
    {
      tmp = ALLOCATE (replace_ob_t, TAG_TEMPORARY, "replace_program");
      tmp->ob = current_object;
      tmp->next = obj_list_replace;
      obj_list_replace = tmp;
    }
  tmp->new_prog = new_prog;
  tmp->var_offset = (unsigned short)var_offset;
  free_string_svalue (sp--);
}

#endif /* F_REPLACE_PROGRAM */


#ifdef F_REPLACEABLE
void
f_replaceable (void)
{
  program_t *prog;
  int i, j, num, numignore;
  char **ignore;

  if (st_num_arg == 2)
    {
      numignore = sp->u.arr->size;
      if (numignore)
        ignore = CALLOCATE (numignore, char *, TAG_TEMPORARY, "replaceable");
      else
	      ignore = 0;
      for (i = 0; i < numignore; i++)
        {
          if (sp->u.arr->item[i].type == T_STRING)
            ignore[i] = findstring(SVALUE_STRPTR(&sp->u.arr->item[i]), NULL);
          else
            ignore[i] = 0;
        }
      prog = (sp - 1)->u.ob->prog;
    }
  else
    {
      numignore = 1;
      ignore = CALLOCATE (1, char *, TAG_TEMPORARY, "replaceable");
      ignore[0] = findstring(APPLY_CREATE, NULL);
      prog = sp->u.ob->prog;
    }

  num = prog->num_functions_total;

  for (i = 0; i < num; i++)
    {
      if (prog->function_flags[i] & (NAME_INHERITED | NAME_NO_CODE))
	      continue;
      for (j = 0; j < numignore; j++)
	      if (ignore[j] == prog->function_table[FIND_FUNC_ENTRY (prog, i)->def.f_index].name)
	        break;
      if (j == numignore)
	      break;
    }
  if (st_num_arg == 2)
    free_array ((sp--)->u.arr);
  FREE (ignore);
  free_svalue (sp, "f_replaceable");
  put_number (i == num);
}
#endif
