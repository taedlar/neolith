#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "interpret.h"
#include "lpc/array.h"
#include "lpc/program.h"
#include "rc/rc.h"

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
