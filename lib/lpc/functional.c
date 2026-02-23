#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "src/frame.h"
#include "src/interpret.h"
#include "src/simul_efun.h"
#include "array.h"
#include "functional.h"
#include "object.h"
#include "program.h"
#include "rc.h"
#include "include/function.h"
#include "include/origin.h"

program_t fake_prog = { .name = "<function>", .program_size = 0 };
unsigned char fake_program = F_RETURN;

/*
 * Very similar to push_control_stack() [which see].  The purpose of this is
 * to insert an frame containing the object which defined a function pointer
 * in cases where it would otherwise not be on the call stack.  This 
 * preserves the idea that function pointers calls happen 'through' the
 * object that define the function pointer. 
 * These frames are the ones that show up as <function> in error traces.
 */
static void setup_fake_frame (funptr_t * fun) {

  if (csp == &control_stack[CONFIG_INT (__MAX_CALL_DEPTH__) - 1])
    {
      set_error_state(ES_STACK_FULL);
      error ("***Too deep recursion.");
    }

  csp++;
  csp->caller_type = caller_type;
  csp->framekind = FRAME_FAKE | FRAME_OB_CHANGE;
  csp->fr.funp = fun;
  csp->ob = current_object;
  csp->prev_ob = previous_ob;
  csp->fp = fp;
  csp->prog = current_prog;
  csp->pc = pc;
  pc = (char *) &fake_program;
  csp->function_index_offset = function_index_offset;
  csp->variable_index_offset = variable_index_offset;
  caller_type = ORIGIN_FUNCTION_POINTER;
  csp->num_local_variables = 0;
  current_prog = &fake_prog;
  previous_ob = current_object;
  current_object = fun->hdr.owner;
}

/* Remove a fake frame added by setup_fake_frame().  Basically just a
 * specialized version of pop_control_stack().
 */
static void remove_fake_frame () {

  DEBUG_CHECK (csp == (control_stack - 1), "Popped out of the control stack\n");
  current_object = csp->ob;
  current_prog = csp->prog;
  previous_ob = csp->prev_ob;
  caller_type = csp->caller_type;
  pc = csp->pc;
  fp = csp->fp;
  function_index_offset = csp->function_index_offset;
  variable_index_offset = csp->variable_index_offset;
  csp--;
}

/* num_arg args are on the stack, and the args from the array vec should be
 * put in front of them.  This is so that the order of arguments is logical.
 * 
 * evaluate( (: f, a :), b) -> f(a,b) and not f(b, a) which would happen
 * if we simply pushed the args from vec at this point.  (Note that the
 * old function pointers are broken in this regard)
 */
int merge_arg_lists (int num_arg, array_t * arr, int start) {

  int num_arr_arg = arr->size - start;
  svalue_t *sptr;

  if (num_arr_arg)
    {
      sptr = (sp += num_arr_arg);
      if (num_arg)
        {
          /* We need to do some stack movement so that the order of arguments is logical */
          while (num_arg--)
            {
              *sptr = *(sptr - num_arr_arg);
              sptr--;
            }
        }
      num_arg = arr->size;
      while (--num_arg >= start)
        assign_svalue_no_free (sptr--, &arr->item[num_arg]);
      /* could just return num_arr_arg if num_arg is 0 but .... -Sym */
      return (int) (sp - sptr);
    }
  return num_arg;
}

/**
 * Create an efun function pointer from an opcode and optional arguments.
 * @param opcode the efun opcode
 * @param args optional array of arguments to bind to the function pointer
 * @return the created efun function pointer
 */
funptr_t* make_efun_funp (int opcode, svalue_t * args) {
  funptr_t *funptr;

  funptr = (funptr_t *) DXALLOC (sizeof (funptr_hdr_t) + sizeof (efun_ptr_t), TAG_FUNP, "make_efun_funp");
  funptr->hdr.owner = current_object;
  add_ref (current_object, "make_efun_funp");
  funptr->hdr.type = FP_EFUN;

  funptr->f.efun.index = (function_index_t)opcode;

  if (args->type == T_ARRAY)
    {
      funptr->hdr.args = args->u.arr;
      args->u.arr->ref++;
    }
  else
    funptr->hdr.args = 0;

  funptr->hdr.ref = 1;
  return funptr;
}

/**
 * Create a local function pointer from an index and optional arguments.
 * @param index the local function index
 * @param args optional array of arguments to bind to the function pointer
 * @return the created local function pointer
 */
funptr_t* make_lfun_funp (int index, svalue_t * args) {
  funptr_t *funptr;

  funptr = (funptr_t *) DXALLOC (sizeof (funptr_hdr_t) + sizeof (local_ptr_t), TAG_FUNP, "make_efun_funp");
  funptr->hdr.owner = current_object;
  add_ref (current_object, "make_efun_funp");
  funptr->hdr.type = FP_LOCAL | FP_NOT_BINDABLE;

  funptr->f.local.index = (function_index_t)(index + function_index_offset);

  if (args->type == T_ARRAY)
    {
      funptr->hdr.args = args->u.arr;
      args->u.arr->ref++;
    }
  else
    funptr->hdr.args = 0;

  funptr->hdr.ref = 1;
  return funptr;
}

/**
 * Create a local function pointer from a function name and optional arguments.
 * Looks up the function by name in current_object's program and creates a function pointer.
 * @param name the function name to look up in current_object
 * @param args optional array of arguments to bind to the function pointer
 * @return the created local function pointer, or NULL if function not found
 */
funptr_t* make_lfun_funp_by_name(const char *name, svalue_t *args) {
  funptr_t *funptr;

  if (!current_object || !current_object->prog || !name)
    return NULL;

  // Find the shared string (function must exist in string table to be in program)
  const char *shared_name = findstring(name);
  if (!shared_name)
    return NULL;  // Function name not in string table = doesn't exist

  // Look up the function in the program
  int index, fio, vio;
  program_t *found_prog = find_function(current_object->prog, shared_name, &index, &fio, &vio);
  if (!found_prog)
    return NULL;  // Function not found

  // Create the funptr with runtime index (already includes inheritance offset)
  funptr = (funptr_t *) DXALLOC (sizeof (funptr_hdr_t) + sizeof (local_ptr_t), TAG_FUNP, "make_lfun_funp_by_name");
  funptr->hdr.owner = current_object;
  add_ref (current_object, "make_lfun_funp_by_name");
  funptr->hdr.type = FP_LOCAL | FP_NOT_BINDABLE;

  // Convert compiler index to runtime index, then add inheritance offset
  // found_prog->function_table[index].runtime_index gives runtime index in found_prog
  // Adding fio translates it to runtime index in current_object->prog
  funptr->f.local.index = (function_index_t)(found_prog->function_table[index].runtime_index + fio);

  if (args && args->type == T_ARRAY)
    {
      funptr->hdr.args = args->u.arr;
      args->u.arr->ref++;
    }
  else
    funptr->hdr.args = 0;

  funptr->hdr.ref = 1;
  return funptr;
}

/**
 * Create a simul efun function pointer from an index and optional arguments.
 * @param index the simul efun function index
 * @param args optional array of arguments to bind to the function pointer
 * @return the created simul efun function pointer
 */
funptr_t* make_simul_funp (int index, svalue_t * args) {
  funptr_t *funptr;

  funptr = (funptr_t *) DXALLOC (sizeof (funptr_hdr_t) + sizeof (simul_ptr_t), TAG_FUNP, "make_efun_funp");
  funptr->hdr.owner = current_object;
  add_ref (current_object, "make_efun_funp");
  funptr->hdr.type = FP_SIMUL;

  funptr->f.simul.index = (function_index_t)index;

  if (args->type == T_ARRAY)
    {
      funptr->hdr.args = args->u.arr;
      args->u.arr->ref++;
    }
  else
    funptr->hdr.args = 0;

  funptr->hdr.ref = 1;
  return funptr;
}

/**
 * Create a functional or anonymous function pointer.
 * @param num_arg number of arguments the function takes
 * @param num_local number of local variables (only for anonymous functions)
 * @param len length of the functional code
 * @param args optional array of arguments to bind to the function pointer
 * @param flag FP_NOT_BINDABLE if the function is not bindable
 * @return the created functional or anonymous function pointer
 */
funptr_t* make_functional_funp (int num_arg, int num_local, int len, svalue_t * args, int flag) {
  funptr_t *funptr;

  funptr = (funptr_t *) DXALLOC (sizeof (funptr_hdr_t) + sizeof (functional_t), TAG_FUNP, "make_functional_funp");
  funptr->hdr.owner = current_object;
  add_ref (current_object, "make_functional_funp");
  funptr->hdr.type = (short)(FP_FUNCTIONAL | flag);

  current_prog->func_ref++;

  funptr->f.functional.prog = current_prog;
  funptr->f.functional.offset = (short)(pc - current_prog->program);
  funptr->f.functional.num_arg = (unsigned char)num_arg;
  funptr->f.functional.num_local = (unsigned char)num_local;
  funptr->f.functional.fio = (short)function_index_offset;
  funptr->f.functional.vio = (short)variable_index_offset;
  pc += len;

  if (args && args->type == T_ARRAY)
    {
      funptr->hdr.args = args->u.arr;
      args->u.arr->ref++;
      funptr->f.functional.num_arg += (unsigned char)args->u.arr->size;
    }
  else
    funptr->hdr.args = 0;

  funptr->hdr.ref = 1;
  return funptr;
}

void push_refed_funp (funptr_t * funptr) {
  sp++;
  sp->type = T_FUNCTION;
  sp->u.fp = funptr;
}

void push_funp (funptr_t * funptr) {
  sp++;
  sp->type = T_FUNCTION;
  sp->u.fp = funptr;
  funptr->hdr.ref++;
}

svalue_t* call_function_pointer (funptr_t * funp, int num_arg) {

  if (funp->hdr.owner->flags & O_DESTRUCTED)
    error ("*Owner (/%s) of function pointer is destructed.", funp->hdr.owner->name);

  setup_fake_frame (funp);

  switch (funp->hdr.type)
    {
    case FP_SIMUL:
      if (funp->hdr.args)
        {
          check_for_destr (funp->hdr.args);
          num_arg = merge_arg_lists (num_arg, funp->hdr.args, 0);
        }
      call_simul_efun (funp->f.simul.index, num_arg);
      break;
    case FP_EFUN:
      {
        int i, def;

        fp = sp - num_arg + 1;
        if (funp->hdr.args)
          {
            check_for_destr (funp->hdr.args);
            num_arg = merge_arg_lists (num_arg, funp->hdr.args, 0);
          }
        i = funp->f.efun.index;
        if (num_arg == instrs[i].min_arg - 1 &&
            ((def = instrs[i].Default) != DEFAULT_NONE))
          {
            if (def == DEFAULT_THIS_OBJECT)
              {
                if (current_object && !(current_object->flags & O_DESTRUCTED))
                  push_object (current_object);
                else
                  *(++sp) = const0;
              }
            else
              {
                (++sp)->type = T_NUMBER;
                sp->u.number = def;
              }
            num_arg++;
          }
        else if (num_arg < instrs[i].min_arg)
          {
            error ("*Too few arguments to efun %s in efun pointer.", instrs[i].name);
          }
        else if (num_arg > instrs[i].max_arg && instrs[i].max_arg != -1)
          {
            error ("*Too many arguments to efun %s in efun pointer.", instrs[i].name);
          }
        /* possibly we should add TRACE, OPC, etc here;
           also on eval_cost here, which is ok for just 1 efun */
        {
          int j, n = num_arg;
          st_num_arg = num_arg;

          if (n >= 4 || instrs[i].max_arg == -1)
            n = instrs[i].min_arg;

          for (j = 0; j < n; j++)
            {
              CHECK_TYPES (sp - num_arg + j + 1, instrs[i].type[j], j + 1, i);
            }
          call_efun (i); // (*efun_table[i - BASE]) ();

          free_svalue (&apply_ret_value, "call_function_pointer");
          if (instrs[i].ret_type == TYPE_NOVALUE)
            apply_ret_value = const0;
          else
            apply_ret_value = *sp--;
          remove_fake_frame ();
          return &apply_ret_value;
        }
      }
    case FP_LOCAL | FP_NOT_BINDABLE:
      {
        compiler_function_t *func;
        fp = sp - num_arg + 1;

        if (current_object->prog->
            function_flags[funp->f.local.index] & NAME_UNDEFINED)
          error ("*Undefined function: %s", function_name (current_object->prog, funp->f.local.index));

        push_control_stack (FRAME_FUNCTION);
        current_prog = funp->hdr.owner->prog;

        caller_type = ORIGIN_LOCAL;
        if (funp->hdr.args)
          {
            array_t *v = funp->hdr.args;

            check_for_destr (v);
            num_arg = merge_arg_lists (num_arg, v, 0);
          }

        csp->num_local_variables = num_arg;
        func = setup_new_frame (funp->f.local.index);

        opt_trace (TT_COMM|2, "Calling local function pointer %s::%s with %d args.\n",
                   current_object->name, func->name, num_arg);
        call_program (current_prog, func->address);
        break;
      }
    case FP_FUNCTIONAL:
    case FP_FUNCTIONAL | FP_NOT_BINDABLE:
      {
        fp = sp - num_arg + 1;

        push_control_stack (FRAME_FUNP);
        current_prog = funp->f.functional.prog;
        csp->fr.funp = funp;

        caller_type = ORIGIN_FUNCTIONAL;

        if (funp->hdr.args)
          {
            array_t *v = funp->hdr.args;

            check_for_destr (v);
            num_arg = merge_arg_lists (num_arg, v, 0);
          }

        setup_variables (num_arg, funp->f.functional.num_local, funp->f.functional.num_arg);

        function_index_offset = funp->f.functional.fio;
        variable_index_offset = funp->f.functional.vio;
        call_program (funp->f.functional.prog, funp->f.functional.offset);
        break;
      }
    default:
      error ("*Unsupported function pointer type.");
    }
  free_svalue (&apply_ret_value, "call_function_pointer");
  apply_ret_value = *sp--;
  remove_fake_frame ();
  return &apply_ret_value;
}

svalue_t *
safe_call_function_pointer (funptr_t * funp, int num_arg)
{
  error_context_t econ;
  svalue_t *ret;

  if (!save_context (&econ))
    return 0;

  if (!setjmp (econ.context))
    {
      ret = call_function_pointer (funp, num_arg);
    }
  else
    {
      restore_context (&econ);
      /* condition was restored to where it was when we came in */
      pop_n_elems (num_arg);
      ret = 0;
    }
  pop_context (&econ);
  return ret;
}
