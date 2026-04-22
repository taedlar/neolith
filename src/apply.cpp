#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "rc.h"
#include "apply.h"
#include "frame.h"
#include "error_context.h"
#include "lpc/object.h"
#include "lpc/program.h"

/*
 * Apply a fun 'fun' to the program in object 'ob', with
 * 'num_arg' arguments (already pushed on the stack).
 * If the function is not found, search in the object pointed to by the
 * inherit pointer.
 * If the function name starts with '::', search in the object pointed out
 * through the inherit pointer by the current object. The 'current_object'
 * stores the base object, not the object that has the current function being
 * evaluated. Thus, the variable current_prog will normally be the same as
 * current_object->prog, but not when executing inherited code. Then,
 * it will point to the code of the inherited object. As more than one
 * object can be inherited, the call of function by index number has to
 * be adjusted. The function number 0 in a superclass object must not remain
 * number 0 when it is inherited from a subclass object. The same problem
 * exists for variables. The global variables function_index_offset and
 * variable_index_offset keep track of how much to adjust the index when
 * executing code in the superclass objects.
 *
 * There is a special case when called from the heart beat, as
 * current_prog will be 0. When it is 0, set current_prog
 * to the 'ob->prog' sent as argument.
 *
 * Arguments are always removed from the stack.
 * If the function is not found, return 0 and nothing on the stack.
 * Otherwise, return 1, and a pushed return value on the stack.
 *
 * Note that the object 'ob' can be destructed. This must be handled by
 * the caller of APPLY_CALL ().
 *
 * If the function failed to be called, then arguments must be deallocated
 * manually !  (Look towards end of this function.)
 */

int call_origin;

static int cache_mask = APPLY_CACHE_SIZE - 1;
#ifdef CACHE_STATS
unsigned int apply_low_call_others = 0;
unsigned int apply_low_cache_hits = 0;
unsigned int apply_low_slots_used = 0;
unsigned int apply_low_collisions = 0;
#endif

typedef struct cache_entry_s {
  int id;
  program_t *oprogp;
  program_t *progp;
  int index;			/* index into progp's function_table */
  shared_str_t name;
  unsigned short num_arg, num_local;
  int function_index_offset;
  int variable_index_offset;

  cache_entry_s() : id(0), oprogp(nullptr), progp(nullptr), index(0), name(nullptr),
                    num_arg(0), num_local(0), function_index_offset(0), variable_index_offset(0) {}
} cache_entry_t;

static cache_entry_t cache[APPLY_CACHE_SIZE];

const char *origin_name (int orig) {
  switch (orig)
    {
    case ORIGIN_DRIVER:
      return "driver";
    case ORIGIN_LOCAL:
      return "local";
    case ORIGIN_CALL_OTHER:
      return "call_other";
    case ORIGIN_SIMUL_EFUN:
      return "simul";
    case ORIGIN_CALL_OUT:
      return "call_out";
    case ORIGIN_EFUN:
      return "efun";
    case ORIGIN_FUNCTION_POINTER:
      return "function pointer";
    case ORIGIN_FUNCTIONAL:
      return "functional";
    default:
      return "(unknown)";
    };
}

/**
 *  @brief Recursive helper for find_function_by_name2().
 *  This was called ffbn_recurse2() in earlier versions.
 *  @param[in] prog The program to search.
 *  @param[in] name The function name to search for. This must be a shared string.
 *  @param[out] index Output parameter for the function index (not runtime index).
 *  @param[out] fio Output parameter for the function index offset.
 *  @param[out] vio Output parameter for the variable index offset.
 *  @return The program_t where the function was found, or NULL if not found.
 */
program_t *find_function (program_t* prog, shared_str_t name, int *index, int *fio, int *vio) {
  int high = prog->num_functions_defined - 1;
  int low = 0;
  int i;

  /* binary search in the function table (in the order of function name shared string pointers, not the name string) */
  while (high >= low)
    {
      int mid = (high + low) / 2;
      char *p = prog->function_table[mid].name;

      if (name < p)
        high = mid - 1;
      else if (name > p)
        low = mid + 1;
      else
        {
          /* TODO: as an optimization, we could use this entry to
           * find the real one, but that requires backtracking all
           * the way up to the top level and back down again.
           * 
           * Instead, for now, we just continue searching.  No need to
           * check the things we inherit, though.
           *
           * NAME_INHERITED is possible in the case of prototype slots that
           * are later replaced by inherited functions.  We could optimize
           * this one fairly easily, but it probably isn't worth checking
           * for separately as it is very rare in normal LPC code (since
           * very little if anything usually precedes inherits).
           */
          int ridx = prog->function_table[mid].runtime_index;
          int flags = prog->function_flags[ridx];
          if (flags & (NAME_UNDEFINED | NAME_PROTOTYPE | NAME_INHERITED))
            {
              if (flags & NAME_INHERITED)
                break;
              return 0;
            }
          *index = mid;
          *fio = 0;
          *vio = 0;
          return prog; /* locally defined function */
        }
    }

  /* Search inherited function tables */
  i = prog->num_inherited;
  while (i--)
    {
      program_t *ret = find_function (prog->inherit[i].prog, name, index, fio, vio);
      if (ret)
        {
          *fio += prog->inherit[i].function_index_offset;
          *vio += prog->inherit[i].variable_index_offset;
          return ret;
        }
    }
  return 0;
}

static program_t *find_function_by_name2 (object_t * ob, const char *name,
                                          shared_str_t *shared_name,
                                          int *index, int *fio, int *vio) {
  *shared_name = findstring(name, NULL); /* shared string */

  if (!*shared_name)
    return 0;
  return find_function (ob->prog, *shared_name, index, fio, vio);
}

static int function_visible (int origin, int func_flags) {
  switch (origin)
    {
    case ORIGIN_LOCAL:
    case ORIGIN_DRIVER:
    case ORIGIN_CALL_OUT:
      break;

    case ORIGIN_CALL_OTHER:
      if (func_flags & (NAME_STATIC | NAME_PRIVATE | NAME_PROTECTED))
        return 0;
      break;

    case ORIGIN_EFUN:
    case ORIGIN_SIMUL_EFUN:
      /*
       *  this should never happen, for simul_efun are called
       *  directly from call_simul_efun()
       */
      break;
    }
  return 1;
}

/**
 * @brief Low-level apply of a function to an object.
 * 
 * This is a hot path in a busy MUD, so it is optimized in several ways.  The main
 * optimization is the apply cache, which caches the results of looking up a function
 * in an object's program.  The cache is indexed by a hash of the program id, the function
 * name pointer, and the function name pointer shifted to ignore low bits that are not
 * significant for hashing).  This allows for very fast lookups in the common case where
 * the same function is called repeatedly on the same object.
 * 
 * In most of the case, the function name pointer is a pointer to a shared string
 * (LPC string literal or identifier), but it can also be a pointer to a regular C string
 * that is not shared (C/C++ string literals, such as APPLY_* in applies.h). In that case,
 * we are still good for using the string pointer for hashing purposes, but we need to convert
 * it to a shared string for the entry->name (and add reference to it) to ensure the string
 * pointer is valid for future comparisons.
 * 
 * @param fun A pointer to the function name. This can be a pointer to a shared string or
 *    a pointer to a regular C string. The address is used for hashing in the apply cache,
 *    so if it is a regular C string, it will be converted to a shared string and the address
 *    of the shared string will be used for hashing and comparisons.
 * @param ob The object to apply the function to.
 * @param num_arg The number of arguments already pushed on the stack.
 * @retval 0 if the applied function is not defined in the LPC object.
 * @retval 1 if the applied function has been called successfully.
 *    The return value from the applied function will be on the stack if 1 is returned.
 */
int apply_low (const char *fun, object_t* ob, int num_arg) {

  if (!ob || (ob->flags & O_DESTRUCTED))
    {
      debug_error ("apply_low: object is destructed\n");
      return 0;
    }
  if (num_arg < 0)
    {
      debug_error ("Negative number of arguments to apply_low: %d\n", num_arg);
      return 0;
    }

  program_t *progp = ob->prog, *prog;
  cache_entry_t *entry = &cache[(progp->id_number ^ (intptr_t) fun ^ ((intptr_t) fun >> APPLY_CACHE_BITS)) & cache_mask];
  int local_call_origin = call_origin;

  if (!local_call_origin)
    local_call_origin = ORIGIN_DRIVER;
  call_origin = 0;

  ob->time_of_ref = current_time;	/* Used by the swapper */

  /*
   * This object will now be used, and is thus a target for reset later on
   * (when time due).
   */
#ifdef LAZY_RESETS
  try_reset (ob);
  if (ob->flags & O_DESTRUCTED)
    {
      pop_n_elems (num_arg);
      return 0;
    }
#endif
  ob->flags &= ~O_RESET_STATE;

#ifdef CACHE_STATS
  apply_low_call_others++;
#endif

  if ((entry->id == progp->id_number) && (entry->oprogp == progp) &&
      (strcmp (entry->name, fun) == 0)) /* entry->name is a shared string, fun is only valid before return */
    {
      /* function entry is found in APPLY_CACHE */
      opt_trace (TT_EVAL, "APPLY_CACHE hit for \"%s\"", fun);

#ifdef CACHE_STATS
      apply_low_cache_hits++;
#endif
      if (entry->progp)
        {
          compiler_function_t *funp = entry->progp->function_table + entry->index;
          int funflags = entry->oprogp->function_flags[funp->runtime_index + entry->function_index_offset];

          /* if progp is zero, the cache is telling us the function
           * isn't here */
          //if (!(funflags & (NAME_STATIC | NAME_PRIVATE))
          //    || (local_call_origin & (ORIGIN_DRIVER | ORIGIN_CALL_OUT)))
          if (function_visible(local_call_origin, funflags))
            {
              /* push a frame onto control stack */
              push_control_stack (FRAME_FUNCTION | FRAME_OB_CHANGE);
              csp->num_local_variables = num_arg;
              csp->fr.table_index = entry->index;

              current_prog = entry->progp;
              caller_type = local_call_origin;
              function_index_offset = entry->function_index_offset;
              variable_index_offset = entry->variable_index_offset;

#ifdef PROFILE_FUNCTIONS
              get_cpu_times (&(csp->entry_secs), &(csp->entry_usecs));
              current_prog->function_table[entry->index].calls++;
#endif

              if (funflags & NAME_TRUE_VARARGS)
                setup_varargs_variables (csp->num_local_variables, entry->num_local, entry->num_arg);
              else
                setup_variables (csp->num_local_variables, entry->num_local, entry->num_arg);

              previous_ob = current_object;
              current_object = ob;
              opt_trace (TT_EVAL, "(cached) calling \"%s\": offset %+d", fun, funp->address);
              call_program (current_prog, funp->address);
              return 1;
            }
        }
       /* when we come here, the cache has told us that the function isn't defined in the object */
    }
  else
    {
      /* function entry is matched in APPLY_CACHE  */
      shared_str_t sfun = nullptr;
      int index, fio, vio;

      opt_trace (TT_EVAL, "APPLY_CACHE miss for \"%s\"", fun);

      /* We are going to search the function in ob and overwrite the cache entry.
       * If the entry->name is not empty, it points to a shared string and we need to release
       * our reference to it before overwriting the entry.
       */
      if (entry->id && entry->name)
        free_string(to_shared_str(entry->name));
#ifdef CACHE_STATS
      if (!entry->id)
        {
          apply_low_slots_used++;
        }
      else
        {
          apply_low_collisions++;
        }
#endif
      prog = find_function_by_name2 (ob, fun, &sfun, &index, &fio, &vio);

      if (prog)
        {
          compiler_function_t *funp = &prog->function_table[index];
          runtime_defined_t *fundefp = &(FIND_FUNC_ENTRY (prog, funp->runtime_index)->def);
          int funflags = ob->prog->function_flags[funp->runtime_index + fio];

          //if (!(funflags & (NAME_STATIC | NAME_PRIVATE))
          //    || (local_call_origin & (ORIGIN_DRIVER | ORIGIN_CALL_OUT)))
          if (function_visible(local_call_origin, funflags))
            {
              push_control_stack (FRAME_FUNCTION | FRAME_OB_CHANGE);
              current_prog = prog;
              caller_type = local_call_origin;

              /* The searched function is found, add to APPLY_CACHE */
              entry->oprogp = ob->prog;
              entry->id = progp->id_number;
              entry->name = ref_string(to_shared_str(sfun));
              entry->index = index;

              csp->fr.table_index = index;
              csp->num_local_variables = num_arg;
              entry->variable_index_offset = variable_index_offset = vio;
              entry->function_index_offset = function_index_offset = fio;
              if (funflags & NAME_TRUE_VARARGS)
                setup_varargs_variables (csp->num_local_variables, fundefp->num_local, fundefp->num_arg);
              else
                setup_variables (csp->num_local_variables, fundefp->num_local, fundefp->num_arg);
              entry->num_arg = fundefp->num_arg;
              entry->num_local = fundefp->num_local;
              entry->progp = current_prog;
              previous_ob = current_object;
              current_object = ob;
              opt_trace (TT_EVAL, "calling \"%s\": offset %+d", fun, funp->address);
              call_program (current_prog, funp->address);

              /*
               * Arguments and local variables are now removed. One
               * resulting value is always returned on the stack.
               */
              return 1;
            }
        } /* prog != NULL */

      /* We have to mark a function not to be in the object */
      entry->id = progp->id_number;
      entry->oprogp = progp;
      entry->name = sfun ? ref_string(to_shared_str(sfun)) : make_shared_string(fun, NULL);
      entry->progp = (program_t *) 0;
    }

  /* Not calling call_program(), pop arguments */
  pop_n_elems (num_arg);

  opt_trace (TT_EVAL|1, "not defined or not visible to caller: \"%s\"", fun);
  return 0;
}

/**
 * @brief Clear the APPLY_CALL () cache.
 */
void clear_apply_cache (void) {
  int i;

  for (i = 0; i < APPLY_CACHE_SIZE; i++)
    {
      if (cache[i].name)
        {
          free_string(to_shared_str(cache[i].name));
        }
      cache[i].id = 0;
      cache[i].oprogp = NULL;
      cache[i].progp = NULL;
      cache[i].name = NULL;
    }
}

/*
 * Arguments are supposed to be
 * pushed (using push_string() etc) before the call. A pointer to a
 * 'svalue_t' will be returned. It will be a null pointer if the called
 * function was not found. Otherwise, the return payload is discarded and a
 * non-null sentinel is returned.
 * Reference counts will be updated for this value, to ensure that no pointers
 * are deallocated.
 */

svalue_t *apply_call (const char *fun, object_t *ob, int num_arg, int where, int with_slot) {
  /*
   * Contract:
   * - Caller has already pushed num_arg args (and optional slot placeholder).
   * - Success returns either a slot pointer (with_slot) or &const1 (non-slot).
   * - Failure returns 0 and leaves slot placeholder intact for *_SLOT_FINISH_CALL().
   */
  IF_DEBUG (svalue_t *expected_sp);
  svalue_t *ret_slot = sp - num_arg;

  if (num_arg < 0)
    error ("*Bad argument count (%d) in apply_call.", num_arg);

  call_origin = where;

  if (with_slot)
    {
      int i;
      svalue_t placeholder = *sp;

      /*
       * Slot wrappers push undefined after arguments.
       * Rotate stack from [args..., slot] to [slot, args...]
       * so apply_low() sees the correct argument list.
       */
      for (i = 0; i < num_arg; i++)
        {
          *(sp - i) = *(sp - i - 1);
        }
      *ret_slot = placeholder;
    }

  IF_DEBUG (expected_sp = sp - num_arg);
  if (apply_low (fun, ob, num_arg) == 0)
    return 0;

  if (with_slot)
    {
      free_svalue (ret_slot, "apply_call");
      *ret_slot = *sp--;
      DEBUG_CHECK (expected_sp != sp, "Corrupt stack pointer.\n");
      return ret_slot;
    }

  free_svalue (sp, "apply_call");
  sp--;
  DEBUG_CHECK (expected_sp != sp, "Corrupt stack pointer.\n");
  return &const1;
}

svalue_t *safe_apply_call (const char *fun, object_t *ob, int num_arg, int where, int with_slot) {
  /*
   * Contract:
   * - Mirrors apply_call() return contract, but swallows driver_runtime_error.
   * - On any early failure/error path, cleans call inputs and recreates slot
   *   placeholder so *_SLOT_FINISH_CALL() remains valid.
   */
  svalue_t *ret = 0;
  error_context_t econ;

  if (num_arg < 0)
    error ("*Bad argument count (%d) in safe_apply_call.", num_arg);

  if (!ob || (ob->flags & O_DESTRUCTED))
    {
      pop_n_elems (num_arg + (with_slot ? 1 : 0));
      if (with_slot)
        push_undefined ();
      return 0;
    }

  try
    {
      neolith::error_boundary_guard boundary (&econ);

      try
        {
          ret = apply_call (fun, ob, num_arg, where, with_slot);
        }
      catch (const neolith::driver_runtime_error &)
        {
          boundary.restore ();
          pop_n_elems (num_arg + (with_slot ? 1 : 0));
          if (with_slot)
            push_undefined ();
          ret = 0;
        }
    }
  catch (const neolith::driver_runtime_error &)
    {
      pop_n_elems (num_arg + (with_slot ? 1 : 0));
      if (with_slot)
        push_undefined ();
      ret = 0;
    }

  return ret;
}

svalue_t *apply_master_ob (const char *fun, int num_arg, int with_slot) {
  /*
   * Contract:
   * - Same stack/slot behavior as apply_call(), but targets master_ob.
   * - Returns (svalue_t *)-1 when master_ob is unavailable.
   */
  svalue_t *ret_slot = sp - num_arg;

  if (num_arg < 0)
    error ("*Bad argument count (%d) in apply_master_ob.", num_arg);

  if (with_slot)
    {
      int i;
      svalue_t placeholder = *sp;

      /* See apply_call(): rotate [args..., slot] to [slot, args...] */
      for (i = 0; i < num_arg; i++)
        {
          *(sp - i) = *(sp - i - 1);
        }
      *ret_slot = placeholder;
    }

  if (!master_ob)
    {
      opt_trace (TT_EVAL, "no master object: \"%s\"", fun);
      pop_n_elems (num_arg);
      return (svalue_t *) - 1;
    }

  call_origin = ORIGIN_DRIVER;
  if (apply_low (fun, master_ob, num_arg) == 0)
    return 0;

  if (with_slot)
    {
      free_svalue (ret_slot, "apply_master_ob");
      *ret_slot = *sp--;
      return ret_slot;
    }

  free_svalue (sp, "apply_master_ob");
  sp--;
  return &const1;
}

svalue_t *safe_apply_master_ob (const char *fun, int num_arg, int with_slot) {
  /*
   * Contract:
   * - Same as safe_apply_call() for master applies.
   * - Preserves slot-wrapper finish contract on missing-master/error paths.
   */
  if (num_arg < 0)
    error ("*Bad argument count (%d) in safe_apply_master_ob.", num_arg);

  if (!master_ob)
    {
      pop_n_elems (num_arg + (with_slot ? 1 : 0));
      if (with_slot)
        push_undefined ();
      return (svalue_t *) - 1;
    }
  return safe_apply_call (fun, master_ob, num_arg, ORIGIN_DRIVER, with_slot);
}

/*
 * May current_object shadow object 'ob' ? We rely heavily on the fact that
 * function names are pointers to shared strings, which means that equality
 * can be tested simply through pointer comparison.
 */
static program_t *ffbn_recurse (program_t * prog, shared_str_t name, int *index, int *runtime_index)
{
  int high = prog->num_functions_defined - 1;
  int low = 0;
  int i;

  /* Search our function table */
  while (high >= low)
    {
      int mid = (high + low) / 2;
      char *p = prog->function_table[mid].name;

      if (name < p)
        high = mid - 1;
      else if (name > p)
        low = mid + 1;
      else
        {
          int ridx = prog->function_table[mid].runtime_index;
          int flags = prog->function_flags[ridx];
          if (flags & (NAME_UNDEFINED | NAME_PROTOTYPE))
            {
              if (flags & NAME_INHERITED)
                break;
              return 0;
            }
          *index = mid;
          *runtime_index = prog->function_table[mid].runtime_index;
          return prog;
        }
    }

  /* Search inherited function tables */
  i = prog->num_inherited;
  while (i--)
    {
      program_t *ret =
        ffbn_recurse (prog->inherit[i].prog, name, index, runtime_index);
      if (ret)
        {
          *runtime_index += prog->inherit[i].function_index_offset;
          return ret;
        }
    }
  return 0;
}

static program_t *find_function_by_name (object_t * ob, const char *name, int *index, int *runtime_index)
{
  shared_str_t funname = findstring(name, NULL);

  if (!funname)
    return 0;
  return ffbn_recurse (ob->prog, funname, index, runtime_index);
}

/*
 * This function is similar to APPLY_CALL (), except that it will not
 * call the function, only return object name if the function exists,
 * or 0 otherwise.  If flag is nonzero, then we admit static and private
 * functions exist.  Note that if you actually intend to call the function,
 * it's faster to just try to call it and check if APPLY_CALL () returns zero.
 */
shared_str_t
function_exists (const char *fun, object_t * ob, int flag)
{
  int index, runtime_index;
  program_t *prog;
  //compiler_function_t *cfp;

  DEBUG_CHECK (ob->flags & O_DESTRUCTED, "function_exists() on destructed object\n");

  if (fun[0] == APPLY___INIT_SPECIAL_CHAR)
    return 0;

  prog = find_function_by_name (ob, fun, &index, &runtime_index);
  if (!prog)
    return 0;

  //cfp = prog->function_table + index;

  if ((ob->prog->function_flags[runtime_index] & NAME_UNDEFINED) ||
      ((ob->prog->function_flags[runtime_index] & (NAME_STATIC | NAME_PRIVATE))
       && current_object != ob && !flag))
    return 0;

  return prog->name;
}
