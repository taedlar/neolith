#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "src/comm.h"
#include "file_utils.h"
#include "src/interpret.h"
#include "lpc/otable.h"
#include "rc.h"
#include "lpc/array.h"
#include "lpc/buffer.h"
#include "lpc/object.h"
#include "lpc/functional.h"
#include "lpc/mapping.h"
#include "lpc/program.h"
#include "lpc/include/function.h"

#include "call_out.h"
#include "sprintf.h"
#include "dumpstat.h"


#ifdef F_ERROR
void f_error (void) {
  int l = SVALUE_STRLEN (sp);
  char err_buf[2048];

  if (sp->u.string[l - 1] == '\n')
    l--;
  if (l > 2045)
    l = 2045;

  err_buf[0] = '*';
  strncpy (err_buf + 1, sp->u.string, l);
  err_buf[l + 1] = '\n';
  err_buf[l + 2] = 0;

  error_handler (err_buf);
}
#endif


#ifdef F_THROW
void f_throw (void) {
  free_svalue (&catch_value, "f_throw");
  catch_value = *sp--;
  throw_error ();		/* do the longjump, with extra checks... */
}
#endif


#ifdef F_CACHE_STATS
static void
print_cache_stats (outbuffer_t * ob)
{
  outbuf_add (ob, "Function cache information\n");
  outbuf_add (ob, "-------------------------------\n");
  outbuf_addv (ob, "%% cache hits:    %10.2f\n",
               100 * ((double) apply_low_cache_hits / apply_low_call_others));
  outbuf_addv (ob, "call_others:     %10lu\n", apply_low_call_others);
  outbuf_addv (ob, "cache hits:      %10lu\n", apply_low_cache_hits);
  outbuf_addv (ob, "cache size:      %10lu\n", APPLY_CACHE_SIZE);
  outbuf_addv (ob, "slots used:      %10lu\n", apply_low_slots_used);
  outbuf_addv (ob, "%% slots used:    %10.2f\n",
               100 * ((double) apply_low_slots_used / APPLY_CACHE_SIZE));
  outbuf_addv (ob, "collisions:      %10lu\n", apply_low_collisions);
  outbuf_addv (ob, "%% collisions:    %10.2f\n",
               100 * ((double) apply_low_collisions / apply_low_call_others));
}

void f_cache_stats (void) {
  outbuffer_t ob;

  outbuf_zero (&ob);
  print_cache_stats (&ob);
  outbuf_push (&ob);
}
#endif


#ifdef F_CALL_STACK
void f_call_stack (void) {
  int i, n = csp - &control_stack[0] + 1;
  array_t *ret;

  if (sp->u.number < 0 || sp->u.number > 3)
    error ("First argument of call_stack() must be 0, 1, 2, or 3.\n");

  ret = allocate_empty_array (n);

  switch (sp->u.number)
    {
    case 0:
      ret->item[0].type = T_STRING;
      ret->item[0].subtype = STRING_MALLOC;
      ret->item[0].u.string = add_slash (current_prog->name);
      for (i = 1; i < n; i++)
        {
          ret->item[i].type = T_STRING;
          ret->item[i].subtype = STRING_MALLOC;
          ret->item[i].u.string = add_slash ((csp - i + 1)->prog->name);
        }
      break;
    case 1:
      ret->item[0].type = T_OBJECT;
      ret->item[0].u.ob = current_object;
      add_ref (current_object, "f_call_stack: curr");
      for (i = 1; i < n; i++)
        {
          ret->item[i].type = T_OBJECT;
          ret->item[i].u.ob = (csp - i + 1)->ob;
          add_ref ((csp - i + 1)->ob, "f_call_stack");
        }
      break;
    case 2:
      for (i = 0; i < n; i++)
        {
          ret->item[i].type = T_STRING;
          if (((csp - i)->framekind & FRAME_MASK) == FRAME_FUNCTION)
            {
              program_t *prog = (i ? (csp - i + 1)->prog : current_prog);
              int index = (csp - i)->fr.table_index;
              compiler_function_t *cfp = &prog->function_table[index];

              ret->item[i].subtype = STRING_SHARED;
              ret->item[i].u.string = cfp->name;
              ref_string (cfp->name);
            }
          else
            {
              ret->item[i].subtype = STRING_CONSTANT;
              ret->item[i].u.string =
                (((csp - i)->framekind & FRAME_MASK) ==
                 FRAME_CATCH) ? "CATCH" : "<function>";
            }
        }
      break;
    case 3:
      ret->item[0].type = T_STRING;
      ret->item[0].subtype = STRING_CONSTANT;
      ret->item[0].u.const_string = origin_name (caller_type);

      for (i = 1; i < n; i++)
        {
          ret->item[i].type = T_STRING;
          ret->item[i].subtype = STRING_CONSTANT;
          ret->item[i].u.const_string = origin_name ((csp - i + 1)->caller_type);
        }
      break;
    }
  put_array (ret);
}
#endif


#ifdef F_FUNCTION_PROFILE
/* f_function_profile: John Garnett, 1993/05/31, 0.9.17.3 */
void
f_function_profile (void)
{
  array_t *vec;
  mapping_t *map;
  program_t *prog;
  int nf, j;

  ob = sp->u.ob;

  prog = ob->prog;
  nf = prog->num_functions_defined;
  vec = allocate_empty_array (nf);
  for (j = 0; j < nf; j++)
    {
      map = allocate_mapping (3);
      add_mapping_pair (map, "calls", prog->function_table[j].calls);
      add_mapping_pair (map, "self", prog->function_table[j].self
                        - prog->function_table[j].children);
      add_mapping_pair (map, "children", prog->function_table[j].children);
      add_mapping_shared_string (map, "name", prog->function_table[j].name);
      vec->item[j].type = T_MAPPING;
      vec->item[j].u.map = map;
    }
  free_object (ob, "f_function_profile");
  put_array (vec);
}
#endif


#ifdef F_LPC_INFO
void
f_lpc_info (void)
{
  outbuffer_t out;

  interface_t **p = interface;
  object_t *ob;

  outbuf_zero (&out);
  outbuf_addv (&out, "%30s  Loaded  Using compiled program\n", "Program");
  while (*p)
    {
      outbuf_addv (&out, "%30s: ", (*p)->fname);
      ob = lookup_object_hash ((*p)->fname);
      if (ob)
        {
          if (ob->flags & O_COMPILED_PROGRAM)
            {
              outbuf_add (&out, " No\n");
            }
          else if (ob->prog->program_size == 0)
            {
              outbuf_add (&out, " Yes      Yes\n");
            }
          else
            {
              outbuf_add (&out, " Yes      No\n");
            }
        }
      else
        {
          outbuf_add (&out,
                      "Something REALLY wierd happened; no record of the object.\n");
        }
      p++;
    }
  outbuf_push (&out);
}
#endif

#ifdef F_MALLOC_STATUS
void
f_malloc_status (void)
{
  outbuffer_t ob;

  outbuf_zero (&ob);

#ifdef BSDMALLOC
  outbuf_add (&ob, "Using BSD malloc");
#endif
#ifdef SMALLOC
  outbuf_add (&ob, "Using Smalloc");
#endif
#ifdef SYSMALLOC
  outbuf_add (&ob, "Using system malloc");
#endif
  outbuf_add (&ob, ".\n");
#ifdef DO_MSTATS
  show_mstats (&ob, "malloc_status()");
#endif
#if (defined(WRAPPEDMALLOC) || defined(DEBUGMALLOC))
  dump_malloc_data (&ob);
#endif
  outbuf_push (&ob);
}
#endif


#ifdef F_MUD_STATUS
void
f_mud_status (void)
{
  int tot, res, verbose = 0;
  outbuffer_t ob;

  outbuf_zero (&ob);
  verbose = (sp--)->u.number;

  if (reserved_area)
    res = CONFIG_INT (__RESERVED_MEM_SIZE__);
  else
    res = 0;

  if (verbose)
    {
      char dir_buf[PATH_MAX];

      outbuf_addv (&ob, "current working directory: %s\n\n",
                   getcwd (dir_buf, PATH_MAX));
      outbuf_add (&ob, "add_message statistics\n");
      outbuf_add (&ob, "------------------------------\n");
      outbuf_addv (&ob,
                   "Calls to add_message: %d   Packets: %d   Average packet size: %f\n\n",
                   add_message_calls, inet_packets,
                   (float) inet_volume / inet_packets);

#ifndef NO_ADD_ACTION
      stat_living_objects (&ob);
      outbuf_add (&ob, "\n");
#endif
#ifdef F_CACHE_STATS
      print_cache_stats (&ob);
      outbuf_add (&ob, "\n");
#endif
      tot = show_otable_status (&ob, verbose);
      outbuf_add (&ob, "\n");
      tot += heart_beat_status (&ob, verbose);
      outbuf_add (&ob, "\n");
      tot += add_string_status (&ob, verbose);
      outbuf_add (&ob, "\n");
      tot += print_call_out_usage (&ob, verbose);
    }
  else
    {
      /* !verbose */
      outbuf_addv (&ob, "Sentences:\t\t\t%8d %8d\n", tot_alloc_sentence,
                   tot_alloc_sentence * sizeof (sentence_t));
      outbuf_addv (&ob, "Objects:\t\t\t%8d %8d\n",
                   tot_alloc_object, tot_alloc_object_size);
      outbuf_addv (&ob, "Prog blocks:\t\t\t%8d %8d\n",
                   total_num_prog_blocks, total_prog_block_size);
#ifdef ARRAY_STATS
      outbuf_addv (&ob, "Arrays:\t\t\t\t%8d %8d\n", num_arrays,
                   total_array_size);
#else
      outbuf_add (&ob,
                  "<Array statistics disabled, no information available>\n");
#endif
      outbuf_addv (&ob, "Mappings:\t\t\t%8d %8d\n", num_mappings,
                   total_mapping_size);
      outbuf_addv (&ob, "Mappings(nodes):\t\t%8d\n", total_mapping_nodes);
      outbuf_addv (&ob, "Interactives:\t\t\t%8d %8d\n", total_users,
                   total_users * sizeof (interactive_t));

      tot = show_otable_status (&ob, verbose) +
        heart_beat_status (&ob, verbose) +
        add_string_status (&ob, verbose) +
        print_call_out_usage (&ob, verbose);
    }

  tot += total_prog_block_size +
#ifdef ARRAY_STATS
    total_array_size +
#endif
    total_mapping_size +
    tot_alloc_sentence * sizeof (sentence_t) +
    tot_alloc_object_size + total_users * sizeof (interactive_t) + res;

  if (!verbose)
    {
      outbuf_add (&ob, "\t\t\t\t\t --------\n");
      outbuf_addv (&ob, "Total:\t\t\t\t\t %8d\n", tot);
    }
  outbuf_push (&ob);
}
#endif


#ifdef F_DUMP_FILE_DESCRIPTORS
void
f_dump_file_descriptors (void)
{
  outbuffer_t out;

  outbuf_zero (&out);
  dump_file_descriptors (&out);
  outbuf_push (&out);
}
#endif


#ifdef F_MEMORY_INFO
void
f_memory_info (void)
{
  int mem;
  object_t *ob;

  if (st_num_arg == 0)
    {
      int res, tot;

      if (reserved_area)
        res = CONFIG_INT (__RESERVED_MEM_SIZE__);
      else
        res = 0;
      tot = total_prog_block_size +
#ifdef ARRAY_STATS
        total_array_size +
#endif
        total_mapping_size +
        tot_alloc_object_size +
        tot_alloc_sentence * sizeof (sentence_t) +
        total_users * sizeof (interactive_t) +
        show_otable_status (0, -1) +
        heart_beat_status (0, -1) +
        add_string_status (0, -1) + print_call_out_usage (0, -1) + res;
      push_number (tot);
      return;
    }
  if (sp->type != T_OBJECT)
    bad_argument (sp, T_OBJECT, 1, F_MEMORY_INFO);
  ob = sp->u.ob;
  if (ob->prog && (ob->prog->ref == 1 || !(ob->flags & O_CLONE)))
    mem = ob->prog->total_size;
  else
    mem = 0;
  mem += (data_size (ob) + sizeof (object_t));
  free_object (ob, "f_memory_info");
  put_number (mem);
}
#endif


#ifdef F_MEMORY_SUMMARY
static int memory_share (svalue_t *);

static int
node_share (mapping_t * m, mapping_node_t * elt, void *tp)
{
  int *t = (int *) tp;
  (void)m; /* unused */

  *t += sizeof (mapping_node_t) - 2 * sizeof (svalue_t);
  *t += memory_share (&elt->values[0]);
  *t += memory_share (&elt->values[1]);

  return 0;
}

static int
memory_share (svalue_t * sv)
{
  int i, total = sizeof (svalue_t);
  int subtotal;

  switch (sv->type)
    {
    case T_STRING:
      switch (sv->subtype)
        {
        case STRING_MALLOC:
          return total +
            (1 + COUNTED_STRLEN (sv->u.string) + sizeof (malloc_block_t)) /
            (COUNTED_REF (sv->u.string));
        case STRING_SHARED:
          return total +
            (1 + COUNTED_STRLEN (sv->u.string) + sizeof (block_t)) /
            (COUNTED_REF (sv->u.string));
        }
      break;
    case T_ARRAY:
    case T_CLASS:
      /* first svalue is stored inside the array struct, so sizeof(array_t)
       * includes one svalue.
       */
      subtotal = sizeof (array_t) - sizeof (svalue_t);
      for (i = 0; i < sv->u.arr->size; i++)
        subtotal += memory_share (&sv->u.arr->item[i]);
      return total + subtotal / sv->u.arr->ref;
    case T_MAPPING:
      subtotal = sizeof (mapping_t);
      mapTraverse (sv->u.map, node_share, &subtotal);
      return total + subtotal / sv->u.map->ref;
    case T_FUNCTION:
      {
        svalue_t tmp;
        tmp.type = T_ARRAY;
        tmp.u.arr = sv->u.fp->hdr.args;

        if (tmp.u.arr)
          subtotal =
            sizeof (funptr_hdr_t) + memory_share (&tmp) - sizeof (svalue_t);
        else
          subtotal = sizeof (funptr_hdr_t);
        switch (sv->u.fp->hdr.type)
          {
          case FP_EFUN:
            subtotal += sizeof (efun_ptr_t);
            break;
          case FP_LOCAL | FP_NOT_BINDABLE:
            subtotal += sizeof (local_ptr_t);
            break;
          case FP_SIMUL:
            subtotal += sizeof (simul_ptr_t);
            break;
          case FP_FUNCTIONAL:
          case FP_FUNCTIONAL | FP_NOT_BINDABLE:
            subtotal += sizeof (functional_t);
            break;
          }
        return total + subtotal / sv->u.fp->hdr.ref;
      }
    case T_BUFFER:
      /* first byte is stored inside the buffer struct */
      return total + (sizeof (buffer_t) + sv->u.buf->size -
                      1) / sv->u.buf->ref;
    }
  return total;
}


/*
 * The returned mapping is:
 * 
 * map["program name"]["variable name"] = memory usage
 */
static void
fms_recurse (mapping_t * map, object_t * ob, int *idx, program_t * prog)
{
  int i;
  svalue_t *entry;
  svalue_t sv;

  sv.type = T_STRING;
  sv.subtype = STRING_SHARED;

  for (i = 0; i < prog->num_inherited; i++)
    fms_recurse (map, ob, idx, prog->inherit[i].prog);

  for (i = 0; i < prog->num_variables_defined; i++)
    {
      int size = memory_share (ob->variables + *idx + i);

      sv.u.string = prog->variable_table[i];
      entry = find_for_insert (map, &sv, 0);
      entry->u.number += size;
    }
  *idx += prog->num_variables_defined;
}

void
f_memory_summary (void)
{
  mapping_t *result = allocate_mapping (8);
  object_t *ob;
  int idx;
  svalue_t sv;

  sv.type = T_STRING;
  sv.subtype = STRING_SHARED;

  for (ob = obj_list; ob; ob = ob->next_all)
    {
      svalue_t *entry;

      sv.u.string = ob->prog->name;
      entry = find_for_insert (result, &sv, 0);
      if (entry->type == T_NUMBER)
        {
          entry->type = T_MAPPING;
          entry->u.map = allocate_mapping (8);
        }
      idx = 0;
      fms_recurse (entry->u.map, ob, &idx, ob->prog);
    }
  push_refed_mapping (result);
}
#endif /* F_MEMORY_SUMMARY */

#ifdef F_DEBUG_INFO
void
f_debug_info (void)
{
  svalue_t *arg;
  outbuffer_t out;
  object_t *ob;

  outbuf_zero (&out);
  arg = sp - 1;
  switch (arg[0].u.number)
    {
    case 0:
      {
        int i, flags;
        object_t *obj2;

        ob = arg[1].u.ob;
        flags = ob->flags;
        outbuf_addv (&out, "O_HEART_BEAT      : %s\n",
                     flags & O_HEART_BEAT ? "TRUE" : "FALSE");
        outbuf_addv (&out, "O_IS_WIZARD       : %s\n",
                     flags & O_IS_WIZARD ? "TRUE" : "FALSE");
        outbuf_addv (&out, "O_ENABLE_COMMANDS : %s\n",
                     flags & O_ENABLE_COMMANDS ? "TRUE" : "FALSE");
        outbuf_addv (&out, "O_CLONE           : %s\n",
                     flags & O_CLONE ? "TRUE" : "FALSE");
        outbuf_addv (&out, "O_VIRTUAL         : %s\n",
                     flags & O_VIRTUAL ? "TRUE" : "FALSE");
        outbuf_addv (&out, "O_DESTRUCTED      : %s\n",
                     flags & O_DESTRUCTED ? "TRUE" : "FALSE");
        outbuf_addv (&out, "O_ONCE_INTERACTIVE: %s\n",
                     flags & O_ONCE_INTERACTIVE ? "TRUE" : "FALSE");
        outbuf_addv (&out, "O_RESET_STATE     : %s\n",
                     flags & O_RESET_STATE ? "TRUE" : "FALSE");
        outbuf_addv (&out, "O_WILL_CLEAN_UP   : %s\n",
                     flags & O_WILL_CLEAN_UP ? "TRUE" : "FALSE");
        outbuf_addv (&out, "O_WILL_RESET: %s\n",
                     flags & O_WILL_RESET ? "TRUE" : "FALSE");
        outbuf_addv (&out, "next_reset  : %d\n", ob->next_reset);
        outbuf_addv (&out, "time_of_ref : %d\n", ob->time_of_ref);
        outbuf_addv (&out, "ref         : %d\n", ob->ref);
        outbuf_addv (&out, "name        : '/%s'\n", ob->name);
        outbuf_addv (&out, "next_all    : OBJ(/%s)\n",
                     ob->next_all ? ob->next_all->name : "NULL");
        if (obj_list == ob)
          outbuf_add (&out, "This object is the head of the object list.\n");
        for (obj2 = obj_list, i = 1; obj2; obj2 = obj2->next_all, i++)
          if (obj2->next_all == ob)
            {
              outbuf_addv (&out, "Previous object in object list: OBJ(/%s)\n",
                           obj2->name);
              outbuf_addv (&out, "position in object list:%d\n", i);
            }
        break;
      }
    case 1:
      ob = arg[1].u.ob;

      outbuf_addv (&out, "program ref's %d\n", ob->prog->ref);
      outbuf_addv (&out, "Name /%s\n", ob->prog->name);
      outbuf_addv (&out, "program size %d\n", ob->prog->program_size);
      outbuf_addv (&out, "runtime function table %d (%d) \n",
                   ob->prog->num_functions_total,
                   ob->prog->num_functions_total *
                   (sizeof (runtime_function_u) + 1));
      outbuf_addv (&out, "compiler function table %d (%d) \n",
                   ob->prog->num_functions_defined,
                   ob->prog->num_functions_defined *
                   sizeof (compiler_function_t));
      outbuf_addv (&out, "num strings %d\n", ob->prog->num_strings);
      outbuf_addv (&out, "num vars %d (%d)\n",
                   ob->prog->num_variables_defined,
                   ob->prog->num_variables_defined * (sizeof (char *) +
                                                      sizeof (short)));
      outbuf_addv (&out, "num inherits %d (%d)\n", ob->prog->num_inherited,
                   ob->prog->num_inherited * sizeof (inherit_t));
      outbuf_addv (&out, "total size %d\n", ob->prog->total_size);
      break;
    case 2:
      {
        int i;
        ob = arg[1].u.ob;
        for (i = 0; i < ob->prog->num_variables_total; i++)
          {
            /* inefficient, but: */
            outbuf_addv (&out, "%s: ", variable_name (ob->prog, i));
            svalue_to_string (&ob->variables[i], &out, 2, 0, 0);
            outbuf_add (&out, "\n");
          }
        break;
      }
    default:
      bad_arg (1, F_DEBUG_INFO);
    }
  pop_stack ();
  pop_stack ();
  outbuf_push (&out);
}
#endif

#ifdef F_REFS
void
f_refs (void)
{
  int r;

  switch (sp->type)
    {
    case T_MAPPING:
      r = sp->u.map->ref;
      break;
    case T_CLASS:
    case T_ARRAY:
      r = sp->u.arr->ref;
      break;
    case T_OBJECT:
      r = sp->u.ob->ref;
      break;
    case T_FUNCTION:
      r = sp->u.fp->hdr.ref;
      break;
    case T_BUFFER:
      r = sp->u.buf->ref;
      break;
    default:
      r = 0;
      break;
    }
  free_svalue (sp, "f_refs");
  put_number (r - 1);		/* minus 1 to compensate for being arg of
                                 * refs() */
}
#endif

#if (defined(DEBUGMALLOC) && defined(DEBUGMALLOC_EXTENSIONS))
#ifdef F_DEBUGMALLOC
void
f_debugmalloc (void)
{
  char *res;

  res = dump_debugmalloc ((sp - 1)->u.string, sp->u.number);
  free_string_svalue (--sp);
  sp->subtype = STRING_MALLOC;
  sp->u.string = res;
}
#endif

#ifdef F_SET_MALLOC_MASK
void
f_set_malloc_mask (void)
{
  set_malloc_mask ((sp--)->u.number);
}
#endif

#ifdef F_CHECK_MEMORY
void
f_check_memory (void)
{
  check_all_blocks ((sp--)->u.number);
}
#endif
#endif /* (defined(DEBUGMALLOC) &&
        * defined(DEBUGMALLOC_EXTENSIONS)) */

#ifdef F_TRACE
void
f_trace (void)
{
  int ot = -1;

  if (command_giver && command_giver->interactive)
    {
      ot = command_giver->interactive->trace_level;
      command_giver->interactive->trace_level = sp->u.number;
    }
  sp->u.number = ot;
}
#endif

#ifdef F_TRACEPREFIX
void
f_traceprefix (void)
{
  char *old = 0;

  if (command_giver && command_giver->interactive)
    {
      old = command_giver->interactive->trace_prefix;
      if (sp->type & T_STRING)
        {
          command_giver->interactive->trace_prefix =
            make_shared_string (sp->u.string);
          free_string_svalue (sp);
        }
      else
        command_giver->interactive->trace_prefix = 0;
    }
  if (old)
    {
      put_shared_string (old);
    }
  else
    *sp = const0;
}
#endif


#ifdef F_PROGRAM_INFO
void
f_program_info (void)
{
  int func_size = 0;
  int string_size = 0;
  int var_size = 0;
  int inherit_size = 0;
  int prog_size = 0;
  int hdr_size = 0;
  int class_size = 0;
  int type_size = 0;
  int total_size = 0;
  object_t *ob;
  program_t *prog;
  outbuffer_t out;
  int i, n;

  if (st_num_arg == 1)
    {
      ob = sp->u.ob;
      prog = ob->prog;
      if (!(ob->flags & O_CLONE))
        {
          hdr_size += sizeof (program_t);
          prog_size += prog->program_size;
          func_size += 2 * prog->num_functions_total;	/* function flags */
#ifdef COMPRESS_FUNCTION_TABLES
          /* compressed table header */
          func_size += sizeof (compressed_offset_table_t) - 1;
          /* it's entries */
          func_size +=
            (prog->function_compressed->first_defined -
             prog->function_compressed->num_compressed);
          /* offset table */
          func_size +=
            sizeof (runtime_function_u) * (prog->num_functions_total -
                                           prog->function_compressed->
                                           num_deleted);
#else
          /* offset table */
          func_size +=
            prog->num_functions_total * sizeof (runtime_function_u);
#endif
          /* definitions */
          func_size +=
            prog->num_functions_defined * sizeof (compiler_function_t);
          string_size += prog->num_strings * sizeof (char *);
          var_size +=
            prog->num_variables_defined * (sizeof (char *) +
                                           sizeof (unsigned short));
          inherit_size += prog->num_inherited * sizeof (inherit_t);
          if (prog->num_classes)
            class_size +=
              prog->num_classes * sizeof (class_def_t) +
              (prog->classes[prog->num_classes - 1].index +
               prog->classes[prog->num_classes -
                             1].size) * sizeof (class_member_entry_t);
          type_size += prog->num_functions_defined * sizeof (short);
          n = 0;
          for (i = 0; i < prog->num_functions_defined; i++)
            {
              int start;
              unsigned short *ts = prog->type_start;
              int ri;

              if (!ts)
                continue;
              start = ts[i];
              if (start == INDEX_START_NONE)
                continue;
              ri = prog->function_table[i].runtime_index;
              start += FIND_FUNC_ENTRY (prog, ri)->def.num_arg;
              if (start > n)
                n = start;
            }
          type_size += n * sizeof (short);
          total_size += prog->total_size;
        }
      pop_stack ();
    }
  else
    {
      for (ob = obj_list; ob; ob = ob->next_all)
        {
          if (ob->flags & O_CLONE)
            continue;
          prog = ob->prog;
          hdr_size += sizeof (program_t);
          prog_size += prog->program_size;
          func_size += prog->num_functions_total;	/* function flags */
#ifdef COMPRESS_FUNCTION_TABLES
          /* compressed table header */
          func_size += sizeof (compressed_offset_table_t) - 1;
          /* it's entries */
          func_size +=
            (prog->function_compressed->first_defined -
             prog->function_compressed->num_compressed);
          /* offset table */
          func_size +=
            sizeof (runtime_function_u) * (prog->num_functions_total -
                                           prog->function_compressed->
                                           num_deleted);
#else
          /* offset table */
          func_size +=
            prog->num_functions_total * sizeof (runtime_function_u);
#endif
          /* definitions */
          func_size +=
            prog->num_functions_defined * sizeof (compiler_function_t);
          string_size += prog->num_strings * sizeof (char *);
          var_size +=
            prog->num_variables_defined * (sizeof (char *) +
                                           sizeof (unsigned short));
          inherit_size += prog->num_inherited * sizeof (inherit_t);
          if (prog->num_classes)
            class_size +=
              prog->num_classes * sizeof (class_def_t) +
              (prog->classes[prog->num_classes - 1].index +
               prog->classes[prog->num_classes -
                             1].size) * sizeof (class_member_entry_t);
          type_size += prog->num_functions_defined * sizeof (short);
          n = 0;
          for (i = 0; i < prog->num_functions_defined; i++)
            {
              int start;
              int ri;

              unsigned short *ts = prog->type_start;
              if (!ts)
                continue;
              start = ts[i];
              if (start == INDEX_START_NONE)
                continue;
              ri = prog->function_table[i].runtime_index;
              start += FIND_FUNC_ENTRY (prog, ri)->def.num_arg;
              if (start > n)
                n = start;
            }
          type_size += n * sizeof (short);
          total_size += prog->total_size;
        }
    }

  outbuf_zero (&out);

  outbuf_addv (&out, "\nheader size: %i\n", hdr_size);
  outbuf_addv (&out, "code size: %i\n", prog_size);
  outbuf_addv (&out, "function size: %i\n", func_size);
  outbuf_addv (&out, "string size: %i\n", string_size);
  outbuf_addv (&out, "var size: %i\n", var_size);
  outbuf_addv (&out, "class size: %i\n", class_size);
  outbuf_addv (&out, "inherit size: %i\n", inherit_size);
  outbuf_addv (&out, "saved type size: %i\n\n", type_size);

  outbuf_addv (&out, "total size: %i\n", total_size);

  outbuf_push (&out);
}
#endif


#ifdef F_DEBUG_MESSAGE
void
f_debug_message (void)
{
  debug_message ("%s", sp->u.string);
  free_string_svalue (sp--);
}
#endif
