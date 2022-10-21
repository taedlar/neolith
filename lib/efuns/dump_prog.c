#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "src/main.h"
#include "src/interpret.h"
#include "src/simulate.h"
#include "lpc/disassemble.h"
#include "lpc/object.h"
#include "src/program.h"

#ifdef F_DUMP_PROG
/*
 * Dump information about a program, optionally disassembling it.
 */

static void dump_prog (program_t *, char *, int);

void
f_dump_prog (void)
{
  program_t *prog;
  char *where;
  int d;
  object_t *ob;
  int narg = st_num_arg;

  if (st_num_arg == 2)
    {
      ob = sp[-1].u.ob;
      d = sp->u.number;
      where = 0;
    }
  else if (st_num_arg == 3)
    {
      ob = sp[-2].u.ob;
      d = sp[-1].u.number;
      where = (sp->type == T_STRING) ? sp->u.string : 0;
    }
  else
    {
      ob = sp->u.ob;
      d = 0;
      where = 0;
    }
  if (!(prog = ob->prog))
    {
      error ("No program for object.\n");
    }
  else
    {
      if (!where)
	{
	  where = "/PROG_DUMP";
	}
      dump_prog (prog, where, d);
    }
  pop_n_elems (narg);
}

/* Current flags:
 * 1 - do disassembly
 * 2 - dump line number table
 */
static void
dump_prog (program_t * prog, char *fn, int flags)
{
  char *fname;
  FILE *f;
  int i, j;

  fname = check_valid_path (fn, current_object, "dumpallobj", 1);
  if (!fname)
    {
      error ("Invalid path '%s' for writing.\n", fn);
      return;
    }
  f = fopen (fname, "w");
  if (!f)
    {
      error ("Unable to open '/%s' for writing.\n", fname);
      return;
    }
  fprintf (f, "NAME: /%s\n", prog->name);
  fprintf (f, "INHERITS:\n");
  fprintf (f, "\tname                    fio    vio\n");
  fprintf (f, "\t----------------        ---    ---\n");
  for (i = 0; i < (int) prog->num_inherited; i++)
    fprintf (f, "\t%-20s  %5d  %5d\n",
	     prog->inherit[i].prog->name,
	     (int) prog->inherit[i].function_index_offset,
	     (int) prog->inherit[i].variable_index_offset);
  fprintf (f, "PROGRAM:");
  for (i = 0; i < (int) prog->program_size; i++)
    {
      if (i % 16 == 0)
	fprintf (f, "\n\t%04x: ", (unsigned int) i);
      fprintf (f, "%02d ", (unsigned char) prog->program[i]);
    }
  fputc ('\n', f);
  fprintf (f, "FUNCTIONS:\n");
  fprintf (f,
	   "      name                  offset  flags  fio  # locals  # args\n");
  fprintf (f,
	   "      --------------------- ------ ------- ---  --------  ------\n");
  for (i = 0; i < prog->num_functions_total; i++)
    {
      char sflags[8];
      int flags;

      flags = prog->function_flags[i];
      sflags[0] = (flags & NAME_INHERITED) ? 'i' : '-';
      sflags[1] = (flags & NAME_UNDEFINED) ? 'u' : '-';
      sflags[2] = (flags & NAME_STRICT_TYPES) ? 's' : '-';
      sflags[3] = (flags & NAME_PROTOTYPE) ? 'p' : '-';
      sflags[4] = (flags & NAME_DEF_BY_INHERIT) ? 'd' : '-';
      sflags[5] = (flags & NAME_ALIAS) ? 'a' : '-';
      sflags[6] = (flags & NAME_TRUE_VARARGS) ? 'v' : '-';
      sflags[7] = '\0';
      if (flags & NAME_INHERITED)
	{
	  runtime_function_u *func_entry = FIND_FUNC_ENTRY (prog, i);
	  fprintf (f, "%4d: %-20s  %5d  %7s %5d\n",
		   i, function_name (prog, i),
		   func_entry->inh.offset,
		   sflags, func_entry->inh.function_index_offset);
	}
      else
	{
	  runtime_function_u *func_entry = FIND_FUNC_ENTRY (prog, i);
	  fprintf (f, "%4d: %-20s  %5d  %7s      %7d   %5d\n",
		   i, function_name (prog, i),
		   func_entry->def.f_index,
		   sflags,
		   func_entry->def.num_arg, func_entry->def.num_local);
	}
    }
  fprintf (f, "VARIABLES:\n");
  for (i = 0; i < (int) prog->num_variables_defined; i++)
    fprintf (f, "%4d: %-12s\n", i, prog->variable_table[i]);
  fprintf (f, "STRINGS:\n");
  for (i = 0; i < (int) prog->num_strings; i++)
    {
      fprintf (f, "%4d: ", i);
      for (j = 0; j < 32; j++)
	{
	  char c;

	  if (!(c = prog->strings[i][j]))
	    break;
	  else if (c == '\n')
	    fprintf (f, "\\n");
	  else
	    fputc (c, f);
	}
      fputc ('\n', f);
    }

  if (flags & 1)
    {
      fprintf (f, "\n;;;  *** Disassembly ***\n");
      disassemble (f, prog->program, 0, prog->program_size, prog);
    }
  if (flags & 2)
    {
      fprintf (f, "\n;;;  *** Line Number Info ***\n");
      dump_line_numbers (f, prog);
    }
  fclose (f);
}

#endif
