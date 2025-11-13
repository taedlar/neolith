#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

/*  binaries.c

    Code to save loaded LPC objects to binary files (in order to speed
    loading during subsequent runs of the driver).

    This is mostly original code by Darin Johnson.  Ideas came from CD,
    including crdir_fopen().  Feel free to use this code but please keep
    credits intact.
*/

#include <sys/stat.h>
#include <sys/types.h>

#define SUPPRESS_COMPILER_INLINES
#include "src/std.h"
#include "lpc/object.h"
#include "lpc/otable.h"
#include "lpc/include/runtime_config.h"
#include "rc.h"
#include "binaries.h"
#include "qsort.h"
#include "hash.h"

static char *magic_id = "NEOL";
static unsigned int driver_id = 0x20251029; /* increment when driver changes */
static time_t config_id = 0;

static FILE *crdir_fopen(char *);
static void patch_out (program_t *, short *, int);
static void patch_in (program_t *, short *, int);
static int str_case_cmp (char *, char *);
static int check_times (time_t, const char *);
static int locate_in (program_t *);
static int locate_out (program_t *);

void
save_binary (program_t * prog, mem_block_t * includes, mem_block_t * patches)
{
  char file_name_buf[200];
  char *file_name = file_name_buf;
  FILE *f;
  int i, tmp;
  short len;
  program_t *p;
  struct stat st;

  svalue_t *ret;
  char *nm;

  if (!CONFIG_STR (__SAVE_BINARIES_DIR__))
    return; /* do not allow save binary */

  /* NEOLITH-ONLY: Allows save_binary without initialization of virtual stack machine. */
  if (get_machine_state() >= MS_MUDLIB_LIMBO)
    {
      nm = add_slash (prog->name);
      push_malloced_string (nm);
      ret = safe_apply_master_ob (APPLY_VALID_SAVE_BINARY, 1);
      if (!MASTER_APPROVED (ret))
        {
          opt_trace (TT_COMPILE|1, "not approved");
          return;
        }
    }
  if (prog->total_size > (int) USHRT_MAX ||
      includes->current_size > (int) USHRT_MAX)
    /* assume all other sizes ok */
    return;

  strcpy (file_name, CONFIG_STR (__SAVE_BINARIES_DIR__));
  if (file_name[0] == '/')
    file_name++;
  if ((-1 == stat (file_name, &st)) && (ENOENT != errno))
    {
      debug_perror ("stat() failed on save binaries dir", file_name);
      return;
    }
  strcat (file_name, "/");
  strcat (file_name, prog->name);
  len = strlen (file_name);
  file_name[len - 1] = 'b';	/* change .c ending to .b */

  opt_trace (TT_COMPILE|1, "writing to: /%s", file_name);
  if (!(f = crdir_fopen (file_name)))
    {
      debug_perror ("crdir_fopen() failed", file_name);
      return;
    }

  /*
   * Write out preamble.  Includes magic number, etc, all of which must
   * match while loading.
   */
  if (fwrite (magic_id, strlen (magic_id), 1, f) != 1 ||
      fwrite ((char *) &driver_id, sizeof driver_id, 1, f) != 1 ||
      fwrite ((char *) &config_id, sizeof config_id, 1, f) != 1)
    {
      debug_perror ("fwrite()", file_name);
      fclose (f);
      return;
    }
  /*
   * Write out list of include files.
   */
  len = includes->current_size;
  fwrite ((char *) &len, sizeof len, 1, f);
  fwrite (includes->block, includes->current_size, 1, f);

  /*
   * copy and patch program
   */
  p = (program_t *) DXALLOC (prog->total_size, TAG_TEMPORARY, "save_binary");
  /* convert to relative pointers, copy, then convert back */
  locate_out (prog);
  memcpy (p, prog, prog->total_size);
  locate_in (prog);
  if (patches->current_size)
    {
      locate_in (p);
      patch_out (p, (short *) patches->block, patches->current_size / sizeof (short));
      locate_out (p);
    }
  /*
   * write out prog.  The prog structure is mostly setup, but strings will
   * have to be stored specially.
   */
  len = SHARED_STRLEN (p->name);
  fwrite ((char *) &len, sizeof len, 1, f);
  fwrite (p->name, sizeof (char), len, f);
  fwrite ((char *) &p->total_size, sizeof p->total_size, 1, f);
  fwrite ((char *) p, p->total_size, 1, f);
  FREE (p);
  p = prog;

  /* inherit names */
  for (i = 0; i < (int) p->num_inherited; i++)
    {
      len = SHARED_STRLEN (p->inherit[i].prog->name);
      fwrite ((char *) &len, sizeof len, 1, f);
      fwrite (p->inherit[i].prog->name, sizeof (char), len, f);
    }

  /* string table */
  for (i = 0; i < (int) p->num_strings; i++)
    {
      tmp = SHARED_STRLEN (p->strings[i]);
      if (tmp > (int) USHRT_MAX)
        {			/* possible? */
          fclose (f);
          error ("String too long for save_binary.\n");
          return;
        }
      len = tmp;
      fwrite ((char *) &len, sizeof len, 1, f);
      fwrite (p->strings[i], sizeof (char), len, f);
    }

  /* var names */
  for (i = 0; i < (int) p->num_variables_defined; i++)
    {
      len = SHARED_STRLEN (p->variable_table[i]);
      fwrite ((char *) &len, sizeof len, 1, f);
      fwrite (p->variable_table[i], sizeof (char), len, f);
    }

  /* function names */
  for (i = 0; i < (int) p->num_functions_defined; i++)
    {
      len = SHARED_STRLEN (p->function_table[i].name);
      fwrite ((char *) &len, sizeof len, 1, f);
      fwrite (p->function_table[i].name, sizeof (char), len, f);
    }

  /* line_numbers */
  if (p->line_info)
    len = p->file_info[0];
  else
    len = 0;
  fwrite ((char *) &len, sizeof len, 1, f);
  fwrite ((char *) p->file_info, len, 1, f);

  /*
   * patches
   */
  len = patches->current_size;
  fwrite ((char *) &len, sizeof len, 1, f);
  fwrite (patches->block, patches->current_size, 1, f);

  fclose (f);
  opt_trace (TT_COMPILE|1, "done: /%s", file_name);
}				/* save_binary() */

static program_t *comp_prog;

static int
compare_compiler_funcs (int *x, int *y)
{
  char *n1 = comp_prog->function_table[*x].name;
  char *n2 = comp_prog->function_table[*y].name;

  /* make sure #global_init# stays last */
  if (n1[0] == '#')
    {
      if (n2[0] == '#')
        return 0;
      return 1;
    }
  if (n2[0] == '#')
    return -1;

  if (n1 < n2)
    return -1;
  if (n1 > n2)
    return 1;
  return 0;
}

static void
sort_function_table (program_t * prog)
{
  int *temp, *inverse, *sorttmp, *invtmp;
  int i;
  int num = prog->num_functions_defined;

  if (!num)
    return;

  temp = CALLOCATE (num, int, TAG_TEMPORARY, "copy_and_sort_function_table");
  for (i = 0; i < num; i++)
    temp[i] = i;

  comp_prog = prog;
  quickSort (temp, num, sizeof (int), compare_compiler_funcs);

  inverse =
    CALLOCATE (num, int, TAG_TEMPORARY, "copy_and_sort_function_table");
  for (i = 0; i < num; i++)
    inverse[temp[i]] = i;

  /* We're not copying, so we have to do the sort in place.  This is a
   * bit tricky to do based on a permutation table, but can be done.
   *
   * Basically, we figure out how to turn the permutation into n swaps.
   * If anyone has a reference for an algorithm for this, I'd like to
   * know; I made this one up.  The basic idea is to do a swap, and then
   * figure out the correct permutation on the remaining n-1 elements.
   */
  sorttmp =
    CALLOCATE (num, int, TAG_TEMPORARY, "copy_and_sort_function_table");
  invtmp =
    CALLOCATE (num, int, TAG_TEMPORARY, "copy_and_sort_function_table");
  for (i = 0; i < num; i++)
    {
      sorttmp[i] = temp[i];
      invtmp[i] = inverse[i];
    }

  for (i = 0; i < num - 1; i++)
    {				/* moving n-1 of them puts the last one
                                   in place too */
      compiler_function_t cft;
      int where = sorttmp[i];

      if (i == where)		/* Already in the right spot */
        continue;

      cft = prog->function_table[i];
      prog->function_table[i] = prog->function_table[where];
      DEBUG_CHECK (sorttmp[invtmp[i]] != i, "sorttmp is messed up.");
      sorttmp[invtmp[i]] = where;
      invtmp[where] = invtmp[i];
      prog->function_table[where] = cft;
    }

#ifdef COMPRESS_FUNCTION_TABLES
  {
    compressed_offset_table_t *cftp = prog->function_compressed;
    int f_ov = cftp->first_overload;
    int f_def = cftp->first_defined;
    int n_ov = f_def - cftp->num_compressed;
    int n_def = prog->num_functions_total - f_def;
    int n_real = f_def - cftp->num_deleted;

    for (i = 0; i < n_ov; i++)
      {
        int j = cftp->index[i];
        int ri = f_ov + i;
        if (j == 255)
          continue;
        if (!(prog->function_flags[ri] & NAME_INHERITED))
          {
            int oldix = prog->function_offsets[j].def.f_index;
            DEBUG_CHECK (oldix >= num, "Function index out of range");
            prog->function_offsets[j].def.f_index = inverse[oldix];
          }
      }
    for (i = 0; i < n_def; i++)
      {
        int ri = f_def + i;
        if (!(prog->function_flags[ri] & NAME_INHERITED))
          {
            int oldix = prog->function_offsets[n_real + i].def.f_index;
            DEBUG_CHECK (oldix >= num, "Function index out of range");
            prog->function_offsets[n_real + i].def.f_index = inverse[oldix];
          }
      }
  }
#else
  {
    int num_runtime = prog->num_functions_total;
    for (i = 0; i < num_runtime; i++)
      {
        if (!(prog->function_flags[i] & NAME_INHERITED))
          {
            int oldix = prog->function_offsets[i].def.f_index;
            DEBUG_CHECK (oldix >= num, "Function index out of range");
            prog->function_offsets[i].def.f_index = inverse[oldix];
          }
      }
  }
#endif

  if (prog->type_start)
    {
      for (i = 0; i < num; i++)
        prog->type_start[i] = prog->type_start[temp[i]];
    }

  FREE (sorttmp);
  FREE (invtmp);
  FREE (temp);
  FREE (inverse);
}

#define ALLOC_BUF(size) \
    if ((size) > buf_size) { FREE(buf); buf = DXALLOC(buf_size = size, TAG_TEMPORARY, "ALLOC_BUF"); }

/* crude hack to check both .B and .b */
#define OUT_OF_DATE 0

program_t *int_load_binary (const char *name)
{
  char file_name_buf[400];
  char *buf, *iname, *file_name = file_name_buf, *file_name_two =
    &file_name_buf[200];
  int fd;
  FILE *f;
  int i, buf_size, ilen;
  unsigned int bin_driver_id;
  time_t bin_config_id, mtime;
  short len;
  program_t *p, *prog;
  object_t *ob;
  struct stat st;


  /* stuff from prolog() */
  num_parse_error = 0;

  if (!CONFIG_STR(__SAVE_BINARIES_DIR__))
    return OUT_OF_DATE;
  sprintf (file_name, "%s/%s", CONFIG_STR (__SAVE_BINARIES_DIR__), name);
  if (file_name[0] == '/')
    file_name++;
  len = strlen (file_name);
  file_name[len - 1] = 'b';

  comp_flag = 1;
  fd = open (file_name, O_RDONLY);
  if (-1 == fd)
    return OUT_OF_DATE;
  if (fstat (fd, &st) == -1) {
    close (fd);
    return OUT_OF_DATE;
  }
  mtime = st.st_mtime;

  if (!(f = fdopen (fd, "r"))) {
    close (fd);
    return OUT_OF_DATE;
  }
  opt_trace (TT_COMPILE|1, "found saved binary: /%s", file_name);

  /* see if we're out of date with source */
  if (check_times (mtime, name) <= 0)
    {
      opt_trace (TT_COMPILE|1, "out of date (source file newer).");
      fclose (f);
      return OUT_OF_DATE;
    }
  buf = DXALLOC (buf_size = SMALL_STRING_SIZE, TAG_TEMPORARY, "ALLOC_BUF");

  /*
   * Read preamble.  This must match, or we assume a different driver or
   * configuration.
   */
  if (fread (buf, strlen (magic_id), 1, f) != 1 ||
      strncmp (buf, magic_id, strlen (magic_id)) != 0)
    {
      opt_trace (TT_COMPILE|1, "out of date. (bad magic number)\n");
      fclose (f);
      FREE (buf);
      return OUT_OF_DATE;
    }
  if ((fread ((char *) &bin_driver_id, sizeof bin_driver_id, 1, f) != 1 || driver_id != bin_driver_id))
    {
      opt_trace (TT_COMPILE|1, "out of date. (driver changed)\n");
      fclose (f);
      FREE (buf);
      return OUT_OF_DATE;
    }
  if (fread ((char *) &bin_config_id, sizeof bin_config_id, 1, f) != 1 || config_id != bin_config_id)
    {
      opt_trace (TT_COMPILE|1, "out of date. (config file changed)\n");
      fclose (f);
      FREE (buf);
      return OUT_OF_DATE;
    }
  /*
   * read list of includes and check times
   */
  if (fread ((char *) &len, sizeof len, 1, f) != 1)
    {
      opt_trace (TT_COMPILE|1, "failed reading include list size.");
      fclose (f);
      FREE (buf);
      return OUT_OF_DATE;
    }
  ALLOC_BUF (len);
  if (fread (buf, sizeof (char), len, f) != (unsigned int)len)
    {
      opt_trace (TT_COMPILE|1, "failed reading include list.");
      fclose (f);
      FREE (buf);
      return OUT_OF_DATE;
    }
  for (iname = buf; iname < buf + len; iname += strlen (iname) + 1)
    {
      if (check_times (mtime, iname) <= 0)
        {
          opt_trace (TT_COMPILE|1, "out of date (include file is newer).");
          fclose (f);
          FREE (buf);
          return OUT_OF_DATE;
        }
    }

  /* check program name */
  if (fread ((char *) &len, sizeof len, 1, f) != 1)
    {
      opt_trace (TT_COMPILE|1, "failed reading binary name length.");
      fclose (f);
      FREE (buf);
      return OUT_OF_DATE;
    }
  if (len > 0)
    {
      ALLOC_BUF (len + 1);
      if (fread (buf, sizeof (char), len, f) != (unsigned int)len)
        {
          opt_trace (TT_COMPILE|1, "failed reading binary name.");
          fclose (f);
          FREE (buf);
          return OUT_OF_DATE;
        }
      buf[len] = '\0';
      if (strcmp (name, buf) != 0)
        {
          opt_trace (TT_COMPILE|1, "binary name [%d]%s inconsistent with file (%s).", (unsigned int)len, buf, name);
          fclose (f);
          FREE (buf);
          return OUT_OF_DATE;
        }
    }

  /*
   * Read program structure.
   */
  if (fread ((char *) &ilen, sizeof ilen, 1, f) != 1)
    {
      opt_trace (TT_COMPILE|1, "failed reading program struct size");
      fclose (f);
      FREE (buf);
      return OUT_OF_DATE;
    }
  p = (program_t *) DXALLOC (ilen, TAG_PROGRAM, "load_binary");
  if (fread ((char *) p, ilen, 1, f) != 1)
    {
      opt_trace (TT_COMPILE|1, "failed reading program struct");
      fclose (f);
      FREE (buf);
      return OUT_OF_DATE;
    }
  locate_in (p);		/* from swap.c */
  p->name = make_shared_string (name);

  /* Read inherit names and find prog.  Check mod times also. */
  for (i = 0; i < (int) p->num_inherited; i++)
    {
      buf[0] = '\0';
      if (fread ((char *) &len, sizeof len, 1, f) == 1)
        {
          ALLOC_BUF (len + 1);
          if (fread (buf, sizeof (char), len, f) == (unsigned int)len)
            buf[len] = '\0';
        }
      if (!buf[0])
        {
          opt_trace (TT_COMPILE|1, "inherited program name corrupted.");
          fclose (f);
          free_string (p->name);
          FREE (p);
          FREE (buf);
          return OUT_OF_DATE;
        }

      /*
       * Check times against inherited source.  If saved binary of
       * inherited prog exists, check against it also.
       */
      sprintf (file_name_two, "%s/%s", CONFIG_STR (__SAVE_BINARIES_DIR__), buf);
      if (file_name_two[0] == '/')
        file_name_two++;
      len = strlen (file_name_two);
      file_name_two[len - 1] = 'b';
      if (check_times (mtime, buf) <= 0 ||
          check_times (mtime, file_name_two) == 0)
        {			/* ok if -1 */
          opt_trace (TT_COMPILE|1, "out of date (inherited source is newer).");
          fclose (f);
          free_string (p->name);
          FREE (p);
          FREE (buf);
          return OUT_OF_DATE;
        }
      /* find inherited program (maybe load it here?) */
      ob = find_object2 (buf);
      if (!ob)
        {
          opt_trace (TT_COMPILE|1, "saved binary inherits: /%s", buf);
          fclose (f);
          free_string (p->name);
          FREE (p);
          inherit_file = buf;	/* freed elsewhere */
          return 0;
        }
      p->inherit[i].prog = ob->prog;
    }

  /* Read string table */
  for (i = 0; i < (int) p->num_strings; i++)
    {
      if (fread ((char *) &len, sizeof len, 1, f) == 1)
        {
          ALLOC_BUF (len + 1);
          if (fread (buf, sizeof (char), len, f) == (unsigned int)len)
            {
              buf[len] = '\0';
              p->strings[i] = make_shared_string (buf);
              continue;
            }
        }
      opt_trace (TT_COMPILE|1, "string table corrupted.");
      fclose (f);
      free_string (p->name);
      FREE (p);
      FREE (buf);
      /* TODO: free shared strings */
      return OUT_OF_DATE;
    }

  /* var names */
  for (i = 0; i < (int) p->num_variables_defined; i++)
    {
      if (fread ((char *) &len, sizeof len, 1, f) == 1)
        {
          ALLOC_BUF (len + 1);
          if (fread (buf, sizeof (char), len, f) == (unsigned int)len)
            {
              buf[len] = '\0';
              p->variable_table[i] = make_shared_string (buf);
              continue;
            }
        }
      opt_trace (TT_COMPILE|1, "variable table corrupted.");
      fclose (f);
      free_string (p->name);
      FREE (p);
      FREE (buf);
      /* TODO: free shared strings */
      return OUT_OF_DATE;
    }

  /* function names */
  for (i = 0; i < (int) p->num_functions_defined; i++)
    {
      if (fread ((char *) &len, sizeof len, 1, f) == 1)
        {
          ALLOC_BUF (len + 1);
          if (fread (buf, sizeof (char), len, f) == (unsigned int)len)
            {
              buf[len] = '\0';
              p->function_table[i].name = make_shared_string (buf);
              continue;
            }
        }
      opt_trace (TT_COMPILE|1, "function table corrupted.");
      fclose (f);
      free_string (p->name);
      FREE (p);
      FREE (buf);
      /* TODO: free shared strings */
      return OUT_OF_DATE;
    }
  sort_function_table (p);

  /* line numbers */
  if (fread ((char *) &len, sizeof len, 1, f) == 1)
    {
      p->file_info = (unsigned short *) DXALLOC (len, TAG_LINENUMBERS, "load binary");
      if (fread ((char *) p->file_info, len, 1, f) != 1)
        {
          opt_trace (TT_COMPILE|1, "line number info corrupted.");
          fclose (f);
          free_string (p->name);
          FREE (p->file_info);
          FREE (p);
          FREE (buf);
          /* TODO: free shared strings */
          return OUT_OF_DATE;
        }
      p->line_info = (unsigned char *) &p->file_info[p->file_info[1]];
    }


  /* patches */
  if (fread ((char *) &len, sizeof len, 1, f) == 1)
    {
      ALLOC_BUF (len);
      if (fread (buf, len, 1, f) == 1)
        {
          /* fix up some stuff */
          patch_in (p, (short *) buf, len / sizeof (short));
        }
    }

  fclose (f);
  FREE (buf);

  /*
   * Now finish everything up.  (stuff from epilog())
   */
  prog = p;
  prog->id_number = get_id_number ();

  total_prog_block_size += prog->total_size;
  total_num_prog_blocks += 1;

  reference_prog (prog, "load_binary");
  for (i = 0; (unsigned) i < prog->num_inherited; i++)
    {
      reference_prog (prog->inherit[i].prog, "inheritance");
    }

  opt_trace (TT_COMPILE|1, "loaded successfully: %s", file_name);
  return prog;
}

void
init_binaries ()
{
  if (CONFIG_STR(__SAVE_BINARIES_DIR__))
    {
      /* The compiled LPC program contains opcode that uses simul_efun indexes.
       * So we use the modification time of the simul_efun file as part of
       * the config_id to ensure that binaries are recompiled when the
       * simul_efun definitions change.
       */
      if (CONFIG_STR(__SIMUL_EFUN_FILE__))
        {
          struct stat st;
          if (0 == stat (CONFIG_STR(__SIMUL_EFUN_FILE__), &st))
            {
              config_id = st.st_mtime;
            }
        }
      debug_message ("{}\tusing #pragma save_binary with data directory %s", CONFIG_STR(__SAVE_BINARIES_DIR__));
      opt_trace (TT_COMPILE|1, "magic id: \"%s\" (len=%d)", magic_id, (int)strlen(magic_id));
      opt_trace (TT_COMPILE|1, "driver id: %u (len=%d)", driver_id, sizeof(driver_id));
      opt_trace (TT_COMPILE|1, "config id: %lu (len=%d)", config_id, sizeof(config_id));
    }
  else
    {
      debug_message ("{}\tnot using #pragma save_binary");
    }
}

/*
 * Test against modification times.  -1 if file doesn't exist,
 * 0 if out of date, and 1 if it's ok.
 */
static int
check_times (time_t mtime, const char *nm)
{
  struct stat st;

  if (stat (nm, &st) == -1)
    return -1;
  if (st.st_mtime > mtime)
    {
      return 0;
    }
  return 1;
}				/* check_times() */

/*
 * Routines to do some hacking on the program being saved/loaded.
 * Basically to fix up string switch tables, since the alternative
 * would probably need a linear search in f_switch().
 * I set things up so these routines can be used with other things
 * that might need patching.
 */
static void
patch_out (program_t * prog, short *patches, int len)
{
  int i;
  char *p;

  p = prog->program;
  while (len > 0)
    {
      i = patches[--len];
      if (p[i] == F_SWITCH && p[i + 1] >> 4 != 0xf)
        {			/* string switch */
          short offset, break_addr;
          char *s;

          /* replace strings in table with string table indices */
          COPY_SHORT (&offset, p + i + 2);
          COPY_SHORT (&break_addr, p + i + 4);

          while (offset < break_addr)
            {
              COPY_PTR (&s, p + offset);
              /*
               * take advantage of fact that s is in strings table to find
               * it's index.
               */
              if (s == 0)
                s = (char *) (intptr_t) - 1;
              else
                s = (char *) (intptr_t) store_prog_string (s);
              COPY_PTR (p + offset, &s);
              offset += SWITCH_CASE_SIZE;
            }
        }
    }
}				/* patch_out() */

static int
str_case_cmp (char *a, char *b)
{
  char *s1, *s2;

  COPY_PTR (&s1, a);
  COPY_PTR (&s2, b);

  return s1 - s2;
}				/* str_case_cmp() */

static void
patch_in (program_t * prog, short *patches, int len)
{
  int i;
  char *p;

  p = prog->program;
  while (len > 0)
    {
      i = patches[--len];
      if (p[i] == F_SWITCH && p[i + 1] >> 4 != 0xf)
        {			/* string switch */
          short offset, start, break_addr;
          char *s;

          /* replace string indices with string pointers */
          COPY_SHORT (&offset, p + i + 2);
          COPY_SHORT (&break_addr, p + i + 4);

          start = offset;
          while (offset < break_addr)
            {
              COPY_PTR (&s, p + offset);
              /*
               * get real pointer from strings table
               */
              if (s == (char *) -1)
                s = 0;
              else
                s = prog->strings[(intptr_t) s];
              COPY_PTR (p + offset, &s);
              offset += SWITCH_CASE_SIZE;
            }
          /* sort so binary search still works */
          quickSort (&p[start], (break_addr - start) / SWITCH_CASE_SIZE,
                     SWITCH_CASE_SIZE, str_case_cmp);
        }
    }
}				/* patch_in() */

/*
 * open file for writing, creating intermediate directories if needed.
 */
FILE *
crdir_fopen (char *file_name)
{
  char *p;
  struct stat st;
  FILE *ret;

  /*
   * Beek - These directories probably exist most of the time, so let's
   * optimize by trying the fopen first
   */
  if ((ret = fopen (file_name, "wb")) != NULL)
    {
      return ret;
    }
  p = file_name;

  while (*p && (p = (char *) strchr (p, '/')))
    {
      *p = '\0';
      if (stat (file_name, &st) == -1)
        {
          /* make this dir */
          if (mkdir (file_name, 0770) == -1)
            {
              *p = '/';
              return (FILE *) 0;
            }
        }
      *p = '/';
      p++;
    }

  return fopen (file_name, "wb");
}

/*
 * marion - adjust pointers for swap out and later relocate on swap in
 *   program
 *   functions
 *   strings
 *   variable_names
 *   inherit
 *   argument_types
 *   type_start
 */

#define DIFF(x, y) ((char *)(x) - (char *)(y))
#define ADD(x, y) (&(((char *)(y))[(intptr_t)x]))

static int
locate_out (program_t * prog)
{
  if (!prog)
    return 0;
  prog->program = (char *) DIFF (prog->program, prog);
  prog->function_table =
    (compiler_function_t *) DIFF (prog->function_table, prog);
  prog->function_flags = (unsigned short *) DIFF (prog->function_flags, prog);
  prog->function_offsets =
    (runtime_function_u *) DIFF (prog->function_offsets, prog);
#ifdef COMPRESS_FUNCTION_TABLES
  prog->function_compressed =
    (compressed_offset_table_t *) DIFF (prog->function_compressed, prog);
#endif
  prog->strings = (char **) DIFF (prog->strings, prog);
  prog->variable_table = (char **) DIFF (prog->variable_table, prog);
  prog->variable_types = (unsigned short *) DIFF (prog->variable_types, prog);
  prog->inherit = (inherit_t *) DIFF (prog->inherit, prog);
  prog->classes = (class_def_t *) DIFF (prog->classes, prog);
  prog->class_members =
    (class_member_entry_t *) DIFF (prog->class_members, prog);
  if (prog->type_start)
    {
      prog->argument_types =
        (unsigned short *) DIFF (prog->argument_types, prog);
      prog->type_start = (unsigned short *) DIFF (prog->type_start, prog);
    }
  return 1;
}


/*
 * marion - relocate pointers after swap in
 *   program
 *   functions
 *   strings
 *   variable_names
 *   inherit
 *   argument_types
 *   type_start
 */
static int
locate_in (program_t * prog)
{
  if (!prog)
    return 0;
  prog->program = ADD (prog->program, prog);
  prog->function_table =
    (compiler_function_t *) ADD (prog->function_table, prog);
  prog->function_flags = (unsigned short *) ADD (prog->function_flags, prog);
  prog->function_offsets =
    (runtime_function_u *) ADD (prog->function_offsets, prog);
#ifdef COMPRESS_FUNCTION_TABLES
  prog->function_compressed =
    (compressed_offset_table_t *) ADD (prog->function_compressed, prog);
#endif
  prog->strings = (char **) ADD (prog->strings, prog);
  prog->variable_table = (char **) ADD (prog->variable_table, prog);
  prog->variable_types = (unsigned short *) ADD (prog->variable_types, prog);
  prog->inherit = (inherit_t *) ADD (prog->inherit, prog);
  prog->classes = (class_def_t *) ADD (prog->classes, prog);
  prog->class_members =
    (class_member_entry_t *) ADD (prog->class_members, prog);
  if (prog->type_start)
    {
      prog->argument_types =
        (unsigned short *) ADD (prog->argument_types, prog);
      prog->type_start = (unsigned short *) ADD (prog->type_start, prog);
    }
  return 1;
}
