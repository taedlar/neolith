#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "interpret.h"
#include "lpc/array.h"
#include "lpc/functional.h"
#include "lpc/mapping.h"
#include "lpc/object.h"
#include "lpc/program.h"
#include "rc/rc.h"
#include "port/ansi.h"
#include "efuns/sprintf.h"

control_stack_t *control_stack = 0;
control_stack_t *csp;   /* Points to last control frame pushed */

static int error_state = 0;

void set_error_state (int flag) {
  error_state |= flag;
}

int get_error_state (int mask) {
  return (error_state & mask);
}

void clear_error_state () {
  error_state = 0;
}

void push_control_stack (int frkind) {
  if (csp == &control_stack[CONFIG_INT (__MAX_CALL_DEPTH__) - 1])
    {
      error_state |= ES_STACK_FULL;
      error ("***Too deep recursion.");
    }
  csp++;
  csp->caller_type = caller_type;
  csp->ob = current_object;
  csp->framekind = frkind;
  csp->prev_ob = previous_ob;
  csp->fp = fp;
  csp->prog = current_prog;
  csp->pc = pc; // save return location
  csp->function_index_offset = function_index_offset;
  csp->variable_index_offset = variable_index_offset;
}

/*
 * Pop the control stack one element, and restore registers.
 * extern_call must not be modified here, as it is used imediately after pop.
 */
void pop_control_stack () {
  DEBUG_CHECK (csp == (control_stack - 1), "Popped out of the control stack\n");
#ifdef PROFILE_FUNCTIONS
  if ((csp->framekind & FRAME_MASK) == FRAME_FUNCTION)
    {
      long secs, usecs, dsecs;
      compiler_function_t *cfp =
        &current_prog->function_table[csp->fr.table_index];

      get_cpu_times ((unsigned long *) &secs, (unsigned long *) &usecs);
      dsecs = (((secs - csp->entry_secs) * 1000000)
               + (usecs - csp->entry_usecs));
      cfp->self += dsecs;
      if (csp != control_stack)
        {
          if (((csp - 1)->framekind & FRAME_MASK) == FRAME_FUNCTION)
            {
              csp->prog->function_table[(csp - 1)->fr.table_index].children +=
                dsecs;
            }
        }
    }
#endif
  current_object = csp->ob;
  current_prog = csp->prog;
  previous_ob = csp->prev_ob;
  caller_type = csp->caller_type;
  pc = csp->pc; // return
  fp = csp->fp;
  function_index_offset = csp->function_index_offset;
  variable_index_offset = csp->variable_index_offset;
  csp--;
}

/*
 * Argument is the function to execute. If it is defined by inheritance,
 * then search for the real definition, and return it.
 * There is a number of arguments on the stack. Normalize them and initialize
 * local variables, so that the called function is pleased.
 */
void setup_variables (int actual, int local, int num_arg) {
  int tmp;

  if ((tmp = actual - num_arg) > 0)
    {
      /* Remove excessive arguments */
      pop_n_elems (tmp);
      push_undefineds (local);
    }
  else
    {
      /* Correct number of arguments and local variables */
      push_undefineds (local - tmp);
    }
  fp = sp - (csp->num_local_variables = local + num_arg) + 1;
}

void setup_varargs_variables (int actual, int local, int num_arg) {
  array_t *arr;
  if (actual >= num_arg)
    {
      int n = actual - num_arg + 1;
      /* Aggregate excessive arguments */
      arr = allocate_empty_array (n);
      while (n--)
        arr->item[n] = *sp--;
    }
  else
    {
      /* Correct number of arguments and local variables */
      push_undefineds (num_arg - 1 - actual);
      arr = &the_null_array;
    }
  push_refed_array (arr);
  push_undefineds (local);
  fp = sp - (csp->num_local_variables = local + num_arg) + 1;
}

compiler_function_t* setup_new_frame (int index) {

  runtime_function_u *func_entry = FIND_FUNC_ENTRY (current_prog, index);
  function_number_t findex;

  function_index_offset = variable_index_offset = 0;

  while (current_prog->function_flags[index] & NAME_INHERITED)
    {
      int offset = func_entry->inh.offset;
      function_index_offset += current_prog->inherit[offset].function_index_offset;
      variable_index_offset += current_prog->inherit[offset].variable_index_offset;
      current_prog = current_prog->inherit[offset].prog;
      index = func_entry->inh.index;
      func_entry = FIND_FUNC_ENTRY (current_prog, index);
    }

  findex = func_entry->def.f_index;
  csp->fr.table_index = findex;
#ifdef PROFILE_FUNCTIONS
  get_cpu_times (&(csp->entry_secs), &(csp->entry_usecs));
  current_prog->function_table[findex].calls++;
#endif

  /* Remove excessive arguments */
  if (current_prog->function_flags[index] & NAME_TRUE_VARARGS)
    setup_varargs_variables (csp->num_local_variables, func_entry->def.num_local, func_entry->def.num_arg);
  else
    setup_variables (csp->num_local_variables, func_entry->def.num_local, func_entry->def.num_arg);
  return &current_prog->function_table[findex];
}

compiler_function_t* setup_inherited_frame (int index) {

  runtime_function_u *func_entry = FIND_FUNC_ENTRY (current_prog, index);
  function_number_t findex;

  while (current_prog->function_flags[index] & NAME_INHERITED)
    {
      int offset = func_entry->inh.offset;
      function_index_offset += current_prog->inherit[offset].function_index_offset;
      variable_index_offset += current_prog->inherit[offset].variable_index_offset;
      current_prog = current_prog->inherit[offset].prog;
      index = func_entry->inh.index;
      func_entry = FIND_FUNC_ENTRY (current_prog, index);
    }

  findex = func_entry->def.f_index;
  csp->fr.table_index = findex;
#ifdef PROFILE_FUNCTIONS
  get_cpu_times (&(csp->entry_secs), &(csp->entry_usecs));
  current_prog->function_table[findex].calls++;
#endif

  /* Remove excessive arguments */
  if (current_prog->function_flags[index] & NAME_TRUE_VARARGS)
    setup_varargs_variables (csp->num_local_variables, func_entry->def.num_local, func_entry->def.num_arg);
  else
    setup_variables (csp->num_local_variables, func_entry->def.num_local, func_entry->def.num_arg);
  return &current_prog->function_table[findex];
}

/**
 * Execute a 'catch' block.
 * This effectively calls eval_instruction() with a setjmp/longjmp
 * error handling around it.
 * If error or throw is called during the evaluation, control
 * returns here, and the caught value is left on the stack.
 * @param p The program code to execute.
 * @param new_pc_offset The pc offset to continue after the catch block.
 */
void do_catch (const char *p, unsigned short new_pc_offset) {

  error_context_t econ;
  (void)new_pc_offset; /* unused, new program counter is restored by restore_context */

  /*
   * Save some global variables that must be restored separately after a
   * longjmp. The stack will have to be manually popped all the way.
   */
  if (!save_context (&econ))
    error ("*Can't catch too deep recursion error.");

  push_control_stack (FRAME_CATCH);

  if (setjmp (econ.context))
    {
      /*
       * They did a throw() or error. That means that the control stack
       * must be restored manually here.
       */
      restore_context (&econ);
      sp++;
      *sp = catch_value;
      catch_value = const1;

      /* if it's too deep or max eval, we can't let them catch it */
      if (get_error_state (ES_MAX_EVAL_COST))
        {
          pop_context (&econ);
          error ("*Can't catch eval cost too big error.");
        }
      if (get_error_state (ES_STACK_FULL))
        {
          pop_context (&econ);
          error ("*Can't catch too deep recursion error.");
        }
    }
  else
    {
      assign_svalue (&catch_value, &const1);
      /* note, this will work, since csp->extern_call won't be used */
      eval_instruction (p);

      /* if no error, the program counter points to the end of catch block and
       * we continue after it.
       */
    }
  pop_context (&econ);
}


static int find_line (const char *p, const program_t * progp, char **ret_file, int *ret_line) {
  int offset;
  unsigned char *lns;
  short abs_line;
  int file_idx;

  *ret_file = "";
  *ret_line = 0;

  if (!progp)
    return 1;
  if (progp == &fake_prog)
    return 2;

  /*
   * Load line numbers from swap if necessary.  Leave them in memory until
   * look_for_objects_to_swap() swaps them back out, since more errors are
   * likely.
   */
  if (!progp->line_info)
    return 4;

  offset = (int)(p - progp->program);
  if (offset > (int) progp->program_size)
    {
      opt_warn (1, "illegal offset %+d in object /%s", offset, progp->name);
      return 4;
    }

  lns = progp->line_info;
  while (offset > *lns)
    {
      offset -= *lns;
      lns += 3;
    }

  COPY_SHORT (&abs_line, lns + 1);

  if (0 == translate_absolute_line (abs_line, &progp->file_info[2], (progp->file_info[1] - 2) * sizeof(short), &file_idx, ret_line))
    {
      *ret_file = progp->strings[file_idx - 1];
      return 0;
    }

  return 4;
}

void get_line_number_info (char **ret_file, int *ret_line) {
  find_line (pc, current_prog, ret_file, ret_line);
  if (!(*ret_file))
    *ret_file = current_prog->name;
}

char* get_line_number (const char *p, const program_t * progp) {
  static char buf[256];
  int i;
  char *file = "???";
  int line = -1;

  i = find_line (p, progp, &file, &line);

  switch (i)
    {
    case 1:
      strcpy (buf, "(no program)");
      return buf;
    case 2:
      *buf = 0;
      return buf;
    case 3:
      strcpy (buf, "(compiled program)");
      return buf;
    case 4:
      strcpy (buf, "(no line numbers)");
      return buf;
    case 5:
      strcpy (buf, "(includes too deep)");
      return buf;
    }
  if (!file)
    file = progp->name;
  sprintf (buf, "/%s:%d", file, line);
  return buf;
}

typedef struct function_trace_details_s {
  char* name;
  int num_arg;
  int num_local;
  int program_offset;
} function_trace_details_t;

static void get_trace_details (const program_t* prog, int index, function_trace_details_t* ftd) {
  compiler_function_t *cfp = &prog->function_table[index];
  runtime_function_u *func_entry = FIND_FUNC_ENTRY (prog, cfp->runtime_index);

  if (ftd)
    {
      ftd->name = cfp->name;
      ftd->program_offset = cfp->address;
      ftd->num_arg = func_entry->def.num_arg;
      ftd->num_local = func_entry->def.num_local;
    }
}

static void get_explicit_line_number_info (const char *p, program_t * prog, char **ret_file, int *ret_line) {
  find_line (p, prog, ret_file, ret_line);
  if (!(*ret_file))
    *ret_file = prog->name;
}

array_t* get_svalue_trace (int how) {
  control_stack_t *p;
  array_t *v;
  mapping_t *m;
  char *file;
  int line;
  int num_arg = 0, num_local = 0;
  svalue_t *ptr;
  int i, n, n2;
  function_trace_details_t ftd;

  if (current_prog == 0)
    return &the_null_array;
  if (csp < &control_stack[0])
    {
      return &the_null_array;
    }
  v = allocate_empty_array ((csp - &control_stack[0]) + 1);
  for (p = &control_stack[0]; p < csp; p++)
    {
      m = allocate_mapping (6);
      switch (p[0].framekind & FRAME_MASK)
        {
        case FRAME_FUNCTION:
          get_trace_details (p[1].prog, p[0].fr.table_index, &ftd);
          num_arg = ftd.num_arg;
          num_local = ftd.num_local;
          add_mapping_string (m, "function", ftd.name);
          break;
        case FRAME_CATCH:
          add_mapping_string (m, "function", "CATCH");
          num_arg = -1;
          break;
        case FRAME_FAKE:
          add_mapping_string (m, "function", "<function>");
          num_arg = -1;
          break;
        case FRAME_FUNP:
          add_mapping_string (m, "function", "<function>");
          num_arg = p[0].fr.funp->f.functional.num_arg;
          num_local = p[0].fr.funp->f.functional.num_local;
          break;
        }
      add_mapping_string (m, "program", p[1].prog->name);
      add_mapping_object (m, "object", p[1].ob);
      get_explicit_line_number_info (p[1].pc, p[1].prog, &file, &line);
      add_mapping_string (m, "file", file);
      add_mapping_pair (m, "line", line);

      if ((how & DUMP_WITH_ARGS) && (num_arg != -1))
        {
          array_t *v2;

          n = num_arg;
          ptr = p[1].fp;
          v2 = allocate_empty_array (n);
          for (i = 0; i < n; i++)
            {
              assign_svalue_no_free (&v2->item[i], &ptr[i]);
            }
          add_mapping_array (m, "arguments", v2);
          v2->ref--;
        }

      if ((how & DUMP_WITH_LOCALVARS) && num_local > 0 && num_arg != -1)
        {
          array_t *v2;

          n = num_arg;
          n2 = num_local;
          ptr = p[1].fp;
          v2 = allocate_empty_array (n2);
          for (i = 0; i < n2; i++)
            {
              assign_svalue_no_free (&v2->item[i], &ptr[i + n]);
            }
          add_mapping_array (m, "locals", v2);
          v2->ref--;
        }

      v->item[(p - &control_stack[0])].type = T_MAPPING;
      v->item[(p - &control_stack[0])].u.map = m;
    }
  m = allocate_mapping (6);
  switch (p[0].framekind & FRAME_MASK)
    {
    case FRAME_FUNCTION:
      get_trace_details (current_prog, p[0].fr.table_index, &ftd);
      num_arg = ftd.num_arg;
      num_local = ftd.num_local;
      add_mapping_string (m, "function", ftd.name);
      break;
    case FRAME_CATCH:
      add_mapping_string (m, "function", "CATCH");
      num_arg = -1;
      break;
    case FRAME_FAKE:
      add_mapping_string (m, "function", "<function>");
      num_arg = -1;
      break;
    case FRAME_FUNP:
      add_mapping_string (m, "function", "<function>");
      num_arg = p[0].fr.funp->f.functional.num_arg;
      num_local = p[0].fr.funp->f.functional.num_local;
      break;
    }
  add_mapping_string (m, "program", current_prog->name);
  if (current_object)
    add_mapping_object (m, "object", current_object);
  get_line_number_info (&file, &line);
  add_mapping_string (m, "file", file);
  add_mapping_pair (m, "line", line);

  if ((how & DUMP_WITH_ARGS) && (num_arg != -1))
    {
      array_t *v2;

      n = num_arg;
      v2 = allocate_empty_array (n);
      for (i = 0; i < n; i++)
        {
          assign_svalue_no_free (&v2->item[i], &fp[i]);
        }
      add_mapping_array (m, "arguments", v2);
      v2->ref--;
    }

  if ((how & DUMP_WITH_LOCALVARS) && num_local > 0 && num_arg != -1)
    {
      array_t *v2;

      n = num_arg;
      n2 = num_local;
      v2 = allocate_empty_array (n2);
      for (i = 0; i < n2; i++)
        {
          assign_svalue_no_free (&v2->item[i], &fp[i + n]);
        }
      add_mapping_array (m, "locals", v2);
      v2->ref--;
    }

  v->item[(csp - &control_stack[0])].type = T_MAPPING;
  v->item[(csp - &control_stack[0])].u.map = m;
  /* return a reference zero array */
  v->ref--;
  return v;
}


/**
 *  Write out a trace. If there is a heart_beat(), then return the
 *  object that had that heart beat.
 */
char* dump_trace (int how) {
  const control_stack_t *p;
  char *ret = 0;
  int num_arg = -1, num_local = -1;
  svalue_t *ptr;
  int i;
  //int offset = 0;
  function_trace_details_t ftd;

  if (current_prog == 0)
    return 0;

  if (csp < &control_stack[0])
    return 0;

  /* control stack */
  for (p = &control_stack[0]; p < csp; p++)
    {
      switch (p[0].framekind & FRAME_MASK)
        {
        case FRAME_FUNCTION:
          get_trace_details (p[1].prog, p[0].fr.table_index, &ftd);
          num_arg = ftd.num_arg;
          num_local = ftd.num_local;
          log_message (NULL, "\t" YEL "%s()" NOR " at " CYN "%s" NOR ", in program /%s (object %s)\n", ftd.name,
                       get_line_number (p[1].pc, p[1].prog), p[1].prog->name, p[1].ob->name);
          if (strcmp (ftd.name, "heart_beat") == 0)
            ret = p->ob ? p->ob->name : 0;
          break;
        case FRAME_FUNP:
          log_message (NULL, "\t" YEL "(function)" NOR " at " CYN "%s" NOR ", in program /%s (object %s)\n",
                       get_line_number (p[1].pc, p[1].prog), p[1].prog->name, p[1].ob->name);
          num_arg = p[0].fr.funp->f.functional.num_arg;
          num_local = p[0].fr.funp->f.functional.num_local;
          break;
        case FRAME_FAKE:
          log_message (NULL, "\t" YEL "(function)" NOR " at " CYN "%s" NOR ", in program /%s (object %s)\n",
                       get_line_number (p[1].pc, p[1].prog), p[1].prog->name, p[1].ob->name);
          num_arg = -1;
          break;
        case FRAME_CATCH:
          log_message (NULL, "\t" YEL "(catch)" NOR " at " CYN "%s" NOR ", in program /%s (object %s)\n",
                       get_line_number (p[1].pc, p[1].prog), p[1].prog->name, p[1].ob->name);
          num_arg = -1;
          break;
        }

      if ((how & DUMP_WITH_ARGS) && (num_arg != -1))
        {
          outbuffer_t outbuf;

          outbuf_zero (&outbuf);
          ptr = p[1].fp;
          outbuf_add (&outbuf, "\t\targuments: ");
          for (i = 0; i < num_arg; i++)
            {
              svalue_to_string (&ptr[i], &outbuf, 0, (i==num_arg-1) ? 0 :',',
                                SV2STR_NOINDENT | SV2STR_NONEWLINE);
            }
          log_message (NULL, "%s\n", outbuf.buffer);
          FREE_MSTR (outbuf.buffer);
        }

      if ((how & DUMP_WITH_LOCALVARS) && num_local > 0 && num_arg != -1)
        {
          outbuffer_t outbuf;

          outbuf_zero (&outbuf);
          ptr = p[1].fp + num_arg;
          outbuf_add (&outbuf, "\t\tlocal variables: ");
          for (i = 0; i < num_local; i++)
            {
              svalue_to_string (&ptr[i], &outbuf, 0, (i==num_local-1) ? 0 : ',',
                                SV2STR_NOINDENT | SV2STR_NONEWLINE);
            }
          log_message (NULL, "%s\n", outbuf.buffer);
          FREE_MSTR (outbuf.buffer);
        }
    }

  /* current_prog */
  switch (p[0].framekind & FRAME_MASK)
    {
    case FRAME_FUNCTION:
      get_trace_details (current_prog, p[0].fr.table_index, &ftd);
      //offset = ftd.program_offset;
      num_arg = ftd.num_arg;
      num_local = ftd.num_local;
      log_message (NULL, "\t" HIY "%s()" NOR " at " HIC "%s" NOR ", in program /%s (object %s)\n", ftd.name,
                   get_line_number (pc, current_prog), current_prog->name, current_object ? current_object->name : "<none>");
      break;
    case FRAME_FUNP:
      log_message (NULL, "\t" HIY "(function)" NOR " at " HIC "%s" NOR ", in program /%s (object %s)\n",
                   get_line_number (pc, current_prog), current_prog->name, current_object->name);
      num_arg = p[0].fr.funp->f.functional.num_arg;
      num_local = p[0].fr.funp->f.functional.num_local;
      break;
    case FRAME_FAKE:
      log_message (NULL, "\t" HIY "(function)" NOR " at " HIC "%s" NOR ", in program /%s (object %s)\n",
                   get_line_number (pc, current_prog), current_prog->name, current_object->name);
      num_arg = -1;
      break;
    case FRAME_CATCH:
      log_message (NULL, "\t" HIY "(catch)" NOR " at " HIC "%s" NOR ", in program /%s (object %s)\n",
                   get_line_number (pc, current_prog), current_prog->name, current_object->name);
      num_arg = -1;
      break;
    }

  if ((how & DUMP_WITH_ARGS) && (num_arg != -1))
    {
      outbuffer_t outbuf;

      outbuf_zero (&outbuf);
      outbuf_add (&outbuf, "\t\targuments: ");
      for (i = 0; i < num_arg; i++)
        {
          svalue_to_string (&fp[i], &outbuf, 0, (i == num_arg - 1) ? 0 : ',',
                            SV2STR_NOINDENT|SV2STR_NONEWLINE);
        }
      log_message (NULL, "%s\n", outbuf.buffer);
      FREE_MSTR (outbuf.buffer);
    }

  if ((how & DUMP_WITH_LOCALVARS) && num_local > 0 && num_arg != -1)
    {
      outbuffer_t outbuf;

      outbuf_zero (&outbuf);
      ptr = fp + num_arg;
      outbuf_add (&outbuf, "\t\tlocal variables: ");
      for (i = 0; i < num_local; i++)
        {
          svalue_to_string (&ptr[i], &outbuf, 0, (i == num_local - 1) ? 0 : ',',
                            SV2STR_NOINDENT|SV2STR_NONEWLINE);
        }
      log_message (NULL, "%s\n", outbuf.buffer);
      FREE_MSTR (outbuf.buffer);
    }

  //log_message (NULL, "\tdisassembly:\n");
  //disassemble (current_log_file, current_prog->program, offset, offset + 30, current_prog);
  fflush (current_log_file);
  return ret;
}

