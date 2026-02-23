#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "rc.h"
#include "error_context.h"
#include "frame.h"
#include "lpc/lex.h"
#include "lpc/mapping.h"
#include "lpc/object.h"
#include "lpc/program.h"
#include "efuns/sprintf.h"

/* Used to throw an error to a catch */
svalue_t catch_value = { .type = T_NUMBER };

static error_context_t *current_error_context = 0;

/**
 * @brief Save the current virtual machine execution context as current error
 * handling context (after push previous error handling context onto the stack).
 * 
 * Be careful. This assumes there will be a frame pushed right after this,
 * as we use econ->save_csp + 1 to restore.
 * 
 * This is also followed by a setjmp(), so it must not be inlined.
 * Previous error context is pushed and MUST be restored by a pop_context() after
 * current error handling context has finished, no matter if longjmp() occurs or not.
 * 
 * The save_context() and restore_context() function provides a try/catch-like
 * mechanism for LPC runtime errors.
 * 
 * If any LPC runtime error occurs, the error handler does a longjmp() to the
 * saved context (next instruction after the setjmp()), which calls the
 * restore_context() to restore the virtual machine state to the saved one.
 * 
 * @param econ The error context structure to fill in.
 * @return Depth of saved context (non-zero) on success, 0 on failure (too deep recursion).
 */
int save_context (error_context_t * econ) {

  int depth = 0;
  error_context_t* ec;

  if (csp == &control_stack[CONFIG_INT (__MAX_CALL_DEPTH__) - 1])
    {
      /* Attempting to push the frame will give Too deep recursion fail now. */
      return 0;
    }
  econ->save_command_giver = command_giver;
  econ->save_sp = sp;           /* stack pointer */
  econ->save_csp = csp;         /* control stack pointer */
  econ->save_context = current_error_context;

  current_error_context = econ;
  ec = current_error_context;
  while(ec) {
    depth++;
    ec = ec->save_context;
  }
  opt_trace (TT_EVAL, "depth %d", depth);

  return depth;
}

/**
 * @brief Pop the current error handling context off the stack of error
 * handling contexts.
 * 
 * This function must be called after save_context() when the saved context
 * is no longer needed, to restore the previous error handling context.
 * 
 * Failing to do so will result in a bad current_error_context pointing to
 * a dangling structure that may have been deallocated from the stack.
 * 
 * @param econ The error context structure to pop.
 */
void pop_context (error_context_t * econ) {
  int depth = 0;
  error_context_t* ec;

  current_error_context = econ->save_context;
  clear_error_state ();

  ec = current_error_context;
  while(ec) {
    depth++;
    ec = ec->save_context;
  }
  opt_trace (TT_EVAL, "depth %d", depth);
}

/**
 * @brief Restore the LPC virtual machine execution context from a saved error
 * handling context.
 * 
 * This function is called after a longjmp() to the saved context in
 * save_context().
 * 
 * @param econ The error context structure to restore from.
 */
void restore_context (error_context_t * econ) {

  command_giver = econ->save_command_giver;
  DEBUG_CHECK (csp < econ->save_csp, "csp is below econ->csp before unwinding.\n");
  if (csp > econ->save_csp)
    {
#ifdef PROFILE_FUNCTIONS
      /* PROFILE_FUNCTIONS needs current_prog to be correct in pop_control_stack() */
      if (csp > econ->save_csp + 1)
        {
          csp = econ->save_csp + 1;
          current_prog = (csp + 1)->prog;
        }
      else
#endif
        csp = econ->save_csp + 1; /* Unwind the control stack to the saved position */
      pop_control_stack ();
    }
  pop_n_elems (sp - econ->save_sp);
}

/**
 * @brief error() has been "fixed" so that users can catch and throw them.
 * To catch them nicely, we really have to provide decent error information.
 * Hence, all errors that are to be caught construct a string containing
 * the error message, which is returned as the thrown value.
 * Users can throw their own error values however they choose.
 */
void throw_error () {
  if (current_error_context && ((current_error_context->save_csp + 1)->framekind & FRAME_MASK) == FRAME_CATCH)
    {
      /* error string in catch_value */
      longjmp (current_error_context->context, 1);
    }
  error ("*Throw with no catch.");
}

static volatile int in_error = 0;
static volatile int in_mudlib_error_handler = 0;

static void debug_message_with_location (const char *err) {
  if (current_object && current_prog)
    {
      debug_message ("{\"object\":\"%s\",\"program\":\"%s\",\"line\":\"%s\"}\t%s",
                       current_object->name, current_prog->name, get_line_number (pc, current_prog), err);
    }
  else if (current_object)
    {
      debug_message ("{\"object\":\"%s\"}\t%s", current_object->name, err);
    }
  else
    {
      debug_message ("{}\t%s", err);
    }
}

static void mudlib_error_handler (const char *err, int catch_flag) {
  mapping_t *m;
  char *file;
  int line;
  svalue_t *mret;

  m = allocate_mapping (6);
  add_mapping_string (m, "error", err);
  if (current_prog)
    add_mapping_string (m, "program", current_prog->name);
  if (current_object)
    add_mapping_object (m, "object", current_object);
  add_mapping_array (m, "trace", get_svalue_trace (0));
  get_line_number_info (&file, &line);
  add_mapping_string (m, "file", file);
  add_mapping_pair (m, "line", line);

  push_refed_mapping (m);
  if (catch_flag)
    {
      push_number (1);
      mret = apply_master_ob (APPLY_ERROR_HANDLER, 2);
    }
  else
    {
      mret = apply_master_ob (APPLY_ERROR_HANDLER, 1);
    }

  if ((svalue_t *) - 1 == mret || NULL == mret)
    {
      debug_message_with_location (err);
      dump_trace (g_trace_flag);
    }
  else if (mret->type == T_STRING && *mret->u.string)
    {
      debug_message ("%s", mret->u.string);
    }
}

void error_handler (const char *err) {
  /* in case we're going to longjmp() from load_object or destruct_object */
  reset_destruct_object_limits();
  reset_load_object_limits();

  if (current_error_context &&
      ((current_error_context->save_csp + 1)->framekind & FRAME_MASK) == FRAME_CATCH &&
      !in_fatal_error())
    {
#ifdef LOG_CATCHES
      if (in_mudlib_error_handler)
        {
          debug_message ("{}\t***** error in mudlib error handler (caught)");
          debug_message_with_location (err);
          dump_trace (g_trace_flag);
          in_mudlib_error_handler = 0;
        }
      else
        {
          in_mudlib_error_handler = 1;
          mudlib_error_handler (err, 1);
          in_mudlib_error_handler = 0;
        }
#endif	/* LOG_CATCHES */

      /* free catch_value allocated in last catch if any */
      free_svalue (&catch_value, "caught error");

      /* allocate new catch_value */
      catch_value.type = T_STRING;
      catch_value.subtype = STRING_MALLOC;
      catch_value.u.string = string_copy (err, "caught error");

      /* jump to do_catch */
      if (current_error_context)
        longjmp (current_error_context->context, 1);
    }

  if (in_error)
    {
      debug_message ("{}\t***** New error occured while generating error trace!");
      debug_message_with_location (err);
      dump_trace (g_trace_flag);

      if (current_error_context)
        longjmp (current_error_context->context, 1);
      fatal ("failed longjmp() or no error context for error.");
    }

  in_error = 1;

  if (in_mudlib_error_handler)
    {
      debug_message ("{}\t***** error in mudlib error handler");
      debug_message_with_location (err);
      dump_trace (g_trace_flag);
      in_mudlib_error_handler = 0;
    }
  else
    {
      in_mudlib_error_handler = 1;
      in_error = 0;
      mudlib_error_handler (err, 0);
      in_error = 1;
      in_mudlib_error_handler = 0;
    }

  if (current_heart_beat)
    {
      set_heart_beat (current_heart_beat, 0);
      debug_message ("{}\t----- heart beat in %s turned off\n", current_heart_beat->name);
#if 0
      if (current_heart_beat->interactive)
        add_message (current_heart_beat, _("Your heart beat stops!\n"));
#endif
      current_heart_beat = 0;
    }

  in_error = 0;

  if (current_error_context)
    longjmp (current_error_context->context, 1);
  fatal ("failed longjmp() or no error context for error.");
}

void error (const char *fmt, ...) {
  char msg[8192];
  int len;
  va_list args;

  va_start (args, fmt);
  len = vsnprintf (msg, sizeof(msg)-1, fmt, args);
  if (len > 0 && msg[len-1] != '\n')
    {
      msg[len] = '\n';
      msg[len+1] = 0;
    }
  va_end (args);

  error_handler (msg);
}


void bad_arg (int arg, int instr) {
  error ("*Bad argument %d to %s()", arg, query_opcode_name (instr));
}

static char *type_names[] = {
  "int",
  "string",
  "array",
  "object",
  "mapping",
  "function",
  "float",
  "buffer",
  "class"
};

#define TYPE_CODES_END 0x400
#define TYPE_CODES_START 0x2

const char *type_name (int c) {
  int j = 0;
  int limit = TYPE_CODES_START;

  do 
    {
      if (c & limit)
        return type_names[j];
      j++;
    }
  while (!((limit <<= 1) & TYPE_CODES_END));
  /* Oh crap.  Take some time and figure out what we have. */
  switch (c)
    {
    case T_INVALID:
      return "*invalid*";
    case T_LVALUE:
      return "*lvalue*";
    case T_LVALUE_BYTE:
      return "*lvalue_byte*";
    case T_LVALUE_RANGE:
      return "*lvalue_range*";
    case T_ERROR_HANDLER:
      return "*error_handler*";
    }
  return "*unknown*";
}

void bad_argument (svalue_t * val, int type, int arg, int instr) {
  outbuffer_t outbuf;
  int flag = 0;
  int j = TYPE_CODES_START;
  int k = 0;
  char msg[8192];

  outbuf_zero (&outbuf);
  outbuf_addv (&outbuf, "Bad argument %d to %s%s, Expected: ", arg,
               query_opcode_name (instr), (instr < BASE ? "" : "()"));

  do
    {
      if (type & j)
        {
          if (flag)
            outbuf_add (&outbuf, " or ");
          else
            flag = 1;
          outbuf_add (&outbuf, type_names[k]);
        }
      k++;
    }
  while (!((j <<= 1) & TYPE_CODES_END));

  outbuf_add (&outbuf, " Got: ");
  svalue_to_string (val, &outbuf, 0, 0, SV2STR_NOINDENT|SV2STR_NONEWLINE);
  outbuf_add (&outbuf, ".\n");
  outbuf_fix (&outbuf);
  msg[sizeof(msg)-1] = 0;
  strncpy (msg, outbuf.buffer, sizeof(msg)-1);
  FREE_MSTR (outbuf.buffer);

  error (msg);
}
