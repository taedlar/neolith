/*
    ORIGINAL AUTHOR
        [????-??-??] by Beek, rewritten

    MODIFIED BY
        [2001-06-28] by Annihilator <annihilator@muds.net>, see CVS log.
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "frame.h"
#include "simul_efun.h"
#include "interpret.h"
#include "lpc/object.h"
#include "lpc/include/origin.h"

#include <assert.h>

/*
 * This file rewritten by Beek because it was inefficient and slow.  We
 * now keep track of two mappings:
 *     name -> index       and     index -> function
 * 
 * index->function is used at runtime since it's very fast.  name->index
 * is used at compile time.  It's sorted so we can search it in O(log n)
 * as opposed to a linear search on the function table.  Note that we
 * can't sort the function table b/c then indices wouldn't be preserved
 * across updates.
 *
 * note, the name list holds names for past and present simul_efuns and
 * is now sorted for finding entries faster etc.  The identifier hash
 * table is used at compile time.
 */

int simul_efun_is_loading = 0;

simul_info_t *simuls = 0;
object_t *simul_efun_ob;

/**
 * Entry in the simuls_sorted table.
 */
typedef struct simul_entry_s {
  char *name;
  int index; /* index in the simuls table */
} simul_entry_t;

static simul_entry_t *simuls_sorted = 0; /* The binary search table of simul_efun names */

static size_t num_simuls = 0; /* Number of entries in simul_efun tables (simuls and simuls_sorted). */

/* forward declarations */

static void find_or_add_simul_efun (program_t*, function_number_t, function_index_t);
static void remove_simuls (void);
static void get_simul_efuns (program_t* prog);


void init_simul_efun (const char *file) {

  object_t *new_ob;

  if ((NULL == file) || ('\0' == *file))
    {
      opt_warn (1, "no simul_efun file");
      return;
    }

  simul_efun_is_loading = 1;
  if (NULL == (new_ob = load_object (file, 0)))
    {
      simul_efun_is_loading = 0;
      debug_error ("failed loading simul_efun file");
      return;
    }
  simul_efun_is_loading = 0;
  set_simul_efun (new_ob);
}

/**
 * Remove all current simul efuns from the simuls/simuls_sorted tables and
 * turn off IHE_SIMUL flag of the simul efun name identifier hash.
 */
static void remove_simuls () {
  int i;
  ident_hash_elem_t *ihe;
  /* inactivate all old simul_efuns */
  for (i = 0; i < (int)num_simuls; i++)
    {
      simuls[i].index = 0;
      simuls[i].func = 0;
    }
  for (i = 0; i < (int)num_simuls; i++)
    {
      if ((ihe = lookup_ident (simuls_sorted[i].name)))
        {
          opt_trace (TT_SIMUL_EFUN|3, "removing simul efun #%d: %s", i, simuls_sorted[i].name);
          if (ihe->dn.simul_num != -1)
            ihe->sem_value--;
          ihe->dn.simul_num = -1;
          ihe->token &= ~IHE_SIMUL;
          /* the simul efun could be overriding an efun, do not remove the permanent identifier here */
        }
      free_string (simuls_sorted[i].name); /* reference added by find_or_add_simul_efun() */
    }
}

/**
 * Add all functions in 'prog' as simul efuns.
 * If 'prog' is NULL, remove all simul efuns.
 * @param prog The new program containing the simul efuns to add.
 */
static void get_simul_efuns (program_t* prog) {

  function_index_t i;
  size_t num_new = prog ? prog->num_functions_total : 0; /* total number of functions in prog, including inherited */

  if (num_simuls)
    {
      remove_simuls ();
      if (!num_new)
        {
          opt_trace (TT_SIMUL_EFUN|2, "no new simul efuns, removing all");
          FREE (simuls_sorted);
          simuls_sorted = 0;
          FREE (simuls);
          simuls = 0;
          num_simuls = 0;
          return;
        }
      else
        {
          /* will be resized later */
          simuls_sorted = RESIZE (simuls_sorted, num_simuls + num_new, simul_entry_t, TAG_SIMULS, "get_simul_efuns");
          simuls = RESIZE (simuls, num_simuls + num_new, simul_info_t, TAG_SIMULS, "get_simul_efuns: 2");
        }
    }
  else
    {
      if (num_new)
        {
          simuls_sorted = CALLOCATE (num_new, simul_entry_t, TAG_SIMULS, "get_simul_efuns");
          simuls = CALLOCATE (num_new, simul_info_t, TAG_SIMULS, "get_simul_efuns: 2");
        }
    }

  /* examine the functions in the new program */
  for (i = 0; i < num_new; i++)
    {
      program_t *nprog;
      function_index_t index;
      runtime_function_u *func_entry;

      if (prog->function_flags[i] & (NAME_NO_CODE | NAME_STATIC | NAME_PRIVATE))
        continue;
      nprog = prog;
      index = i;
      func_entry = FIND_FUNC_ENTRY (nprog, index);

      while (nprog->function_flags[index] & NAME_INHERITED)
        {
          nprog = nprog->inherit[func_entry->inh.offset].prog;
          index = func_entry->inh.index;
          func_entry = FIND_FUNC_ENTRY (nprog, index);
        }
      find_or_add_simul_efun (nprog, func_entry->def.f_index, i);
    }

  if (num_simuls)
    {
      /* shrink to fit */
      simuls_sorted = RESIZE (simuls_sorted, num_simuls, simul_entry_t, TAG_SIMULS, "get_simul_efuns");
      simuls = RESIZE (simuls, num_simuls, simul_info_t, TAG_SIMULS, "get_simul_efuns");
    }
}

#define compare_addrs(x,y) (x < y ? -1 : (x > y ? 1 : 0))

int find_simul_efun (const char *name) {
  int first = 0;
  int last = (int)num_simuls - 1;
  int i, j;

  while (first <= last)
    {
      j = (first + last) / 2; /* binary search */
      i = compare_addrs (name, simuls_sorted[j].name);
      if (i == -1)
        {
          last = j - 1;
        }
      else if (i == 1)
        {
          first = j + 1;
        }
      else
        return simuls_sorted[j].index;
    }
  return -1;
}

/**
 * Find or add a simul_efun function.
 * @param prog The program containing the function.
 * @param index The function number in prog's function_table.
 * @param runtime_index The function index in prog's runtime function table.
 */
static void find_or_add_simul_efun (program_t* prog, function_number_t index, function_index_t runtime_index) {

  ident_hash_elem_t *ihe;
  int first = 0;
  int last = (int)num_simuls - 1;
  int i, j;
  compiler_function_t *funp = &prog->function_table[index];

  /* funp->name is a shared string but can be considered permanent as long as
   * the simul_efun_ob is loaded. 
   */

  while (first <= last) /* binary search on address of name in simuls_sorted */
    {
      j = (first + last) / 2;
      i = compare_addrs (funp->name, simuls_sorted[j].name);
      if (i == -1)
        {
          last = j - 1;
        }
      else if (i == 1)
        {
          first = j + 1;
        }
      else /* found: funp->name == simuls_sorted[j].name */
        {
          /* An identifier of the same already exists:
           * In grammar.y, the rule L_DEFINED_NAME '(' expr_list ')' parses in below order:
           * 1. If its a locally defined function (including inherited), use that.
           * 2. Else if its a simul_efun function, use that.
           * 3. Else if its an efun, use that.
           * 4. Else, handle forward declaration or error.
           */
          ihe = find_or_add_perm_ident (simuls_sorted[j].name, IHE_SIMUL);
          DEBUG_CHECK1 (ihe != NULL,
                        "find_or_add_perm_ident() returned NULL for simul_efun '%s'\n",
                        simuls_sorted[j].name);
          ihe->sem_value++;
          ihe->dn.simul_num = (short)simuls_sorted[j].index;
          simuls[simuls_sorted[j].index].index = runtime_index;
          simuls[simuls_sorted[j].index].func = funp;
          opt_trace (TT_SIMUL_EFUN|2, "promoted as simul_efun #%d: %s", simuls_sorted[j].index, funp->name);
          return;
        }
    }
  /* not found, append new entry in simuls and insert to simuls_sorted at position 'first' */
  for (i = (int)num_simuls - 1; i > last; i--)
    simuls_sorted[i + 1] = simuls_sorted[i];
  simuls[num_simuls].index = runtime_index;
  simuls[num_simuls].func = funp;
  simuls_sorted[first].name = funp->name;
  simuls_sorted[first].index = (int)num_simuls;
  opt_trace (TT_SIMUL_EFUN|2, "added simul_efun #%d: %s", simuls_sorted[first].index, funp->name);
  /* update identifier hash, so LPC compiler don't have to call find_simul_efun() */
  ihe = find_or_add_perm_ident (funp->name, IHE_SIMUL);
  DEBUG_CHECK1 (ihe == NULL,
                "find_or_add_perm_ident() returned NULL for simul_efun '%s'\n",
                funp->name);
  ihe->sem_value++;
  ihe->dn.simul_num = (short)num_simuls++; /* new simul_efun */
  ref_string (funp->name); /* will be freed in remove_simuls() */
}

void set_simul_efun (object_t* ob) {

  if (ob && ob->flags & O_DESTRUCTED)
    error ("Bad simul_efun object\n");

  if (simul_efun_ob)
    {
      get_simul_efuns (NULL); /* remove all simul_efuns */
      free_object (simul_efun_ob, "set_simul_efun");
    }

  if (!(simul_efun_ob = ob))
    return;
  get_simul_efuns (simul_efun_ob->prog);
  add_ref (simul_efun_ob, "set_simul_efun");
}

void call_simul_efun (int simul_num, int num_args)
{
  if (simul_num < 0 || simul_num >= (int)num_simuls)
    error ("Bad simul_num %d\n", simul_num);

  if (current_object->flags & O_DESTRUCTED)
    {				/* No external calls allowed */
      opt_trace (TT_SIMUL_EFUN|3, "current_object was destructed, returning undefined");
      pop_n_elems (num_args);
      push_undefined ();
      return;
    }

  if (simuls[simul_num].func)
    {
      /* Don't need to use apply() since we have the pointer directly;
       * this saves function lookup.
       */
      compiler_function_t *funp;
      simul_efun_ob->time_of_ref = current_time;
      push_control_stack (FRAME_FUNCTION | FRAME_OB_CHANGE);
      caller_type = ORIGIN_SIMUL_EFUN;
      csp->num_local_variables = num_args;
      current_prog = simul_efun_ob->prog;
      funp = setup_new_frame (simuls[simul_num].index);
      previous_ob = current_object;
      current_object = simul_efun_ob;
      opt_trace (TT_SIMUL_EFUN|2, "simul_num #%d: %s (num_args=%d)", simul_num, funp->name, num_args);
      call_program (current_prog, funp->address);
    }
  else
    error ("Function is no longer a simul_efun.\n");
}
