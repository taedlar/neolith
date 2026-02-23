#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#define SUPPRESS_COMPILER_INLINES
#include "std.h"
#include "rc.h"
#include "command.h"
#include "interpret.h"
#include "simulate.h"
#include "simul_efun.h"
#include "uids.h"
#include "lpc/array.h"
#include "lpc/functional.h"
#include "lpc/mapping.h"
#include "lpc/object.h"
#include "lpc/operator.h"
#include "lpc/otable.h"
#include "lpc/program.h"
#include "lpc/program/disassemble.h"
#include "lpc/program/binaries.h"
#include "lpc/include/origin.h"
#include "lpc/include/runtime_config.h"
#include "socket/socket_efuns.h"
#include "efuns/call_out.h"
#include "efuns/ed.h"
#include "efuns/file_utils.h"
#include "efuns/replace_program.h"
#include "efuns/sprintf.h"
#include "port/ansi.h"

#include <assert.h>
#include <sys/stat.h>

/*
 * 'inherit_file' is used as a flag. If it is set to a string
 * after yyparse(), this string should be loaded as an object,
 * and the original object must be loaded again.
 */
char *inherit_file;

char *last_verb = 0;

int illegal_sentence_action;

object_t *obj_list, *obj_list_destruct;
object_t *current_object;	/* The object interpreting a function. */
object_t *previous_ob;  /* The object that called the current_object. */
object_t *command_giver;	/* Where the current command came from. */
object_t *current_interactive;	/* The user who caused this execution */

static int give_uid_to_object (object_t *);
static int init_object (object_t *);
static svalue_t *load_virtual_object (const char *);
static char *make_new_name (const char *);
static void send_say (object_t *, char *, array_t *);
static void remove_sent (object_t *, object_t *);


/*************************************************************************
 *  command_giver_stack
 */

static object_t *command_giver_stack[1024];
static object_t **cgsp = command_giver_stack;

void save_command_giver (object_t * new_command_giver) {
  if (cgsp >= EndOf (command_giver_stack))
    fatal ("*****Command giver stack overflow!");

  *(++cgsp) = command_giver;

  if (new_command_giver)
    add_ref (new_command_giver, "save_command_giver");
  command_giver = new_command_giver;
}

void restore_command_giver () {
  if (command_giver)
    free_object (command_giver, "restore_command_giver");

  if (cgsp == command_giver_stack)
    fatal ("*****Command giver stack underflow!");

  command_giver = *(cgsp--);
}

/*********************************************************************/

/**
 * @brief Check that a string is legal for printing.
 * 
 * If the string is too long, an error is raised.
 * @param s The string to check.
 */
void check_legal_string (const char *s) {
  if (strlen (s) >= LARGEST_PRINTABLE_STRING)
    {
      error ("*Printable strings limited to length of %d.\n",
             LARGEST_PRINTABLE_STRING);
    }
}

/* equivalent to strcpy(x, y); return x + strlen(y), but faster and safer */
/* Code like:
 * 
 * char buf[256];
 * strcpy(buf, ...);
 * strcat(buf, ...);
 * strcat(buf, ...);
 *
 * Should be replaced with:
 *
 * char buf[256];
 * char *p, *end = EndOf(buf);
 * p = strput(buf, end, ...);
 * p = strput(p, end, ...);
 * p = strput(p, end, ...);
 */
char *strput (char *x, char *limit, const char *y) {
#ifdef HAVE_STPNCPY
  return stpncpy(x, y, limit - x);
#else
  while ((*x++ = *y++))
    {
      if (x == limit)
        {
          *(x - 1) = 0;
          break;
        }
    }
  return x - 1;
#endif
}

char* strput_int (char *x, char *limit, int num) {
  char buf[20];
  sprintf (buf, "%d", num);
  return strput (x, limit, buf);
}


/**
 *  @brief Give the correct uid and euid to a created object.
 * 
 *  An object must have a uid. The euid may be NULL.
 */
static int give_uid_to_object (object_t * ob) {
  svalue_t *ret;
  char *creator_name = NULL;

  /* before master object is loaded */
  if (get_machine_state() < MS_MUDLIB_LIMBO)
    {
      ob->uid = add_uid ("NONAME");
      ob->euid = NULL;
      return 1;
    }

  /* ask master object who the creator of this object is */
  push_malloced_string (add_slash (ob->name));
  ret = apply_master_ob (APPLY_CREATOR_FILE, 1);

  if (ret == (svalue_t *) - 1)
    {
      destruct_object (ob);
      error ("*Can't load objects without a master object.");
      return 1;
    }

  if (ret && ret->type == T_STRING)
    creator_name = ret->u.string;

  if (!creator_name)
    creator_name = "NONAME";

  /*
   * Now we are sure that we have a creator name. Do not call apply()
   * again, because creator_name will be lost !
   */
  if (current_object)
    {
      if (current_object->uid && strcmp (current_object->uid->name, creator_name) == 0)
        {
          /*
          * The loaded object has the same uid as the loader.
          */
          ob->uid = current_object->uid;
          opt_info (2, "object /%s is granted uid \"%s\" by creator /%s.", ob->name, ob->uid->name, current_object->name);
          return 1;
        }

#ifdef AUTO_TRUST_BACKBONE
      if (backbone_uid && !strcmp (backbone_uid->name, creator_name))
        {
          /*
          * The object is loaded from backbone. This is trusted, so we let it
          * inherit the value of eff_user.
          */
          ob->uid = current_object->euid;
          ob->euid = current_object->euid;
          opt_info (2, "object /%s is granted uid and euid \"%s\" by backbone.", ob->name, ob->uid->name);
          return 1;
        }
#endif
    }

  /*
   * The object is not loaded from backbone, nor from from the loading
   * objects path. That should be an object defined by another wizard. It
   * can't be trusted, so we give it the same uid as the creator. Also give
   * it eff_user 0, which means that user 'a' can't use objects from user
   * 'b' to load new objects nor modify files owned by user 'b'.
   * 
   * If this effect is wanted, user 'b' must let his object do 'seteuid()' to
   * himself. That is the case for most rooms.
   */
  ob->uid = add_uid (creator_name);
  ob->euid = NULL;

  opt_info (2, "object /%s is granted uid \"%s\".", ob->name, ob->uid->name);
  return 1;
}


static int init_object (object_t * ob) {
  return give_uid_to_object (ob);
}


static svalue_t *load_virtual_object (const char *name) {
  svalue_t *v;

  if (get_machine_state() < MS_MUDLIB_LIMBO)
    return 0;
  push_malloced_string (add_slash (name));
  v = apply_master_ob (APPLY_COMPILE_OBJECT, 1);
  if (!v || (v->type != T_OBJECT))
    return 0;
  return v;
}

/**
 * @brief Set the master object.
 * 
 * This function sets the master object for the MUD driver.
 * It also retrieves and assigns the root and backbone user IDs
 * 
 * @param ob The new master object.
 */
void set_master (object_t * ob) {
  int first_load = (!master_ob);
  svalue_t *ret;
  char *uid = NULL;

  if (ob && ob->flags & O_DESTRUCTED)
    error ("Bad master object\n");

  if (master_ob)
    {
      /* release reference to the old master_ob */
      assert (master_ob->ref > 1);
      free_object (master_ob, "set_master");
    }
  if (!(master_ob = ob))
    return;

  /* Make sure master_ob is never made a dangling pointer. */
  add_ref (master_ob, "set_master");
  opt_trace (TT_EVAL|1, "master object ref = %d", master_ob->ref);

  ret = apply_master_ob (APPLY_GET_ROOT_UID, 0);
  if (ret && (ret->type == T_STRING))
    uid = ret->u.string;

  if (first_load)
    {
      if (uid)
        {
          master_ob->uid = set_root_uid (uid);
          master_ob->euid = master_ob->uid;
        }

      /* The backbone UID is set only when the master object is first loaded.
       * If the master object changes later, the backbone UID remains the same
       * because there could be already objects created with that UID.
       * 
       * Retain the original backbone UID to allow new objects created by
       * backbone (as indicated by creator_file) to receive UID and EUID of
       * current_object.
       */
      ret = apply_master_ob (APPLY_GET_BACKBONE_UID, 0);
      if (ret && (ret->type == T_STRING))
        set_backbone_uid (ret->u.string);
    }
  else if (uid)
    {
      master_ob->uid = add_uid (uid);
      master_ob->euid = master_ob->uid;
    }
}


/**
 * @brief Strip leading slashes and check for double slashes in a file name.
 */
static const char *strip_and_check_name (const char *src) {
  const char *p;
  while (*src == '/')
    src++; /* strip leading slashes */
  p = src;
  while (*p)
    {
      if (*p == '/' && *(p + 1) == '/')
        return 0; /* double slash not allowed */
      p++;
    }
  return src;
}

/* prevents infinite inherit loops.
   No, mark-and-sweep solution won't work.  Exercise for reader.  */
static int num_objects_this_thread = 0;

void reset_load_object_limits() {
  num_objects_this_thread = 0;
}

/**
 * @brief Load an object definition from file. If the object wants to inherit
 * from an object that is not loaded, discard all, load the inherited object,
 * and reload again.
 *
 * In mudlib3.0 when loading inherited objects, their reset() is not called.
 * - why is this??  it makes no sense and causes a problem when a developer
 * inherits code from a real used item.  Say a room for example.  In this case
 * the room is loaded but is never set up properly, so when someone enters it
 * it's all messed up.  Realistically, I know that it's pretty bad style to
 * inherit from an object that's actually being used and isn't just a building
 * block, but I see no reason for this limitation.  It happens, and when it
 * does occur, produces mysterious results than can be hard to track down.
 * for now, I've reenabled resetting.  We'll see if anything breaks. -WF
 *
 * Save the command_giver, because reset() in the new object might change it.
 *
 * @param[IN] mudlib_filename The filename of the object to load. Leading slashes
 *            are stripped. Extension ".c" is added if not present. Nested paths
 *            supported (e.g., "path/to/object.c"). The resulting object name
 *            strips leading slashes and ".c" extension.
 * @param[IN] pre_text [NEOLITH-EXTENSION] Optional LPC source code to compile.
 *            If NULL, loads from filesystem. If non-NULL, compiles from this
 *            string and the source file becomes optional. This enables unit
 *            testing without filesystem scaffolding.
 *            Example: load_object("test.c", "void create() { }\n");
 * @return The loaded object, or NULL if it could not be loaded.
 *
 * @note Object naming: "user.c" → name="user", "path/to/obj.c" → name="path/to/obj"
 * @note Requires master_ob to be initialized first (via init_master()).
 * @note Enforces inherit chain depth limit (__INHERIT_CHAIN_SIZE__ config).
 */
object_t* load_object (const char *mudlib_filename, const char *pre_text) {

  int f;
  program_t *prog;
  object_t *ob, *save_command_giver = command_giver;
  svalue_t *mret;
  struct stat c_st;
  char real_name[PATH_MAX], name[PATH_MAX - 2];

  if (++num_objects_this_thread > CONFIG_INT (__INHERIT_CHAIN_SIZE__))
    error ("*Inherit chain too deep: > %d when trying to load '%s'.", CONFIG_INT (__INHERIT_CHAIN_SIZE__), mudlib_filename);

  if (!strip_name (mudlib_filename, name, sizeof (name)))
    error ("*Filenames with consecutive /'s in them aren't allowed (%s).", mudlib_filename);

  if (get_machine_state() >= MS_MUDLIB_LIMBO)
    {
      if (current_object && current_object!=master_ob && current_object->euid == NULL)
        error ("*Can't load objects when no effective user.");
    }

  /*
   * First check that the c-file exists.
   */
  memset (real_name, 0, sizeof (real_name));
  (void) strncpy (real_name, name, sizeof(real_name) - 1);
  (void) strncat (real_name, ".c", sizeof(real_name) - strlen(real_name) - 1);

  opt_trace(TT_COMPILE|1, "load_object: \"%s\"", real_name);
  if (stat (real_name, &c_st) == -1)
    {
      svalue_t *v;

      if ((v = load_virtual_object (name)))
        {
          /* A virtual object is returned by the master object.
          * We don't care about its actual filename, just the object.
          * Replace the object's name with the requested name and update it in the object hash table.
          */
          ob = v->u.ob;
          remove_object_hash (ob);
          if (ob->name)
            FREE (ob->name);
          ob->name = alloc_cstring (name, "load_object");
          enter_object_hash (ob);
          ob->flags |= O_VIRTUAL;
          ob->load_time = current_time;
          num_objects_this_thread--;
          return ob;
        }
      else if (!pre_text)
        {
          num_objects_this_thread--;
          return 0;
        }
    }
  else
    {
      /*
      * Check if it's a legal name.
      */
      if (!legal_path (real_name))
        {
          debug_message ("Illegal pathname: /%s\n", real_name);
          error ("*Illegal path name '/%s'.", real_name);
          return 0;
        }
      opt_trace (TT_COMPILE|2, "legal_path passed: \"%s\"", real_name);
    }

  /* Get the program by loading from binary or compiling from the source */
  if (!(prog = load_binary (real_name)) && !inherit_file)
    {
      opt_trace (TT_COMPILE|2, "no binary found, compiling: \"%s\"", real_name);

      /* maybe move this section into compile_file? */
#ifdef _WIN32
      f = FILE_OPEN (real_name, O_RDONLY | O_TEXT);
#else
      f = FILE_OPEN (real_name, O_RDONLY);
#endif
      if (f == -1 && !pre_text) /* [NEOLITH-EXTENSION] if pre_text is specified, the source file is optional */
        {
          debug_perror ("open()", real_name);
          error ("*Could not read the file '/%s'.", real_name);
        }
      /* compile LPC program from the source, optionally using pre_text */
      prog = compile_file (f, real_name, pre_text);

      update_compile_av (total_lines);
      total_lines = 0;
      if (f != -1)
        FILE_CLOSE (f);
    }

  /* Sorry, can't handle objects without programs yet. */
  if (inherit_file == 0 && (num_parse_error > 0 || prog == 0))
    {
      if (prog)
        free_prog (prog, 1);
      if (num_parse_error == 0 && prog == 0)
        error ("*No program in object '/%s'!", name);
      error ("*Error in loading object '/%s':", name);
    }

  /*
   * This is an iterative process. If this object wants to inherit an
   * unloaded object, then discard current object, load the object to be
   * inherited and reload the current object again. The global variable
   * "inherit_file" will be set by grammar.y to point to a file name.
   */
  if (inherit_file)
    {
      object_t *inh_obj;
      char inhbuf[MAX_OBJECT_NAME_SIZE];

      if (!strip_name (inherit_file, inhbuf, sizeof inhbuf))
        strcpy (inhbuf, inherit_file);

      FREE (inherit_file);
      inherit_file = 0;

      if (prog)
        {
          free_prog (prog, 1);
          prog = 0;
        }
      if (strcmp (inhbuf, name) == 0)
        {
          error ("*Illegal to inherit self.");
        }

      if ((inh_obj = lookup_object_hash (inhbuf)))
        {
          IF_DEBUG (fatal ("*****Inherited object is already loaded!"));
        }
      else
        {
          opt_trace (TT_COMPILE|2, "loading inherit file: /%s", inhbuf);
          inh_obj = load_object (inhbuf, 0);
        }
      if (!inh_obj)
        error ("*Inherited file '/%s' does not exist!", inhbuf);

      /*
       * Yes, the following is necessary.  It is possible that when we
       * loaded the inherited object, it loaded this object from it's
       * create function. Without this check, that would crash the driver.
       * -Beek
       */
      if (!(ob = lookup_object_hash (name)))
        {
          ob = load_object (name, 0);
          /* sigh, loading the inherited file removed us */
          if (!ob)
            {
              num_objects_this_thread--;
              return 0;
            }
          ob->load_time = current_time;
        }
      num_objects_this_thread--;
      return ob;
    }

  opt_trace (TT_COMPILE|2, "creating object: \"/%s\"", name);
  ob = get_empty_object (prog->num_variables_total);
  /* Shared string is no good here */
  ob->name = alloc_cstring (name, "load_object");
  
  ob->prog = prog;
  ob->flags |= O_WILL_RESET;	/* must be before reset is first called */
  ob->next_all = obj_list;
  obj_list = ob;

  opt_trace (TT_COMPILE|2, "adding to otable: \"%s\"", real_name);
  enter_object_hash (ob);	/* add name to fast object lookup table */

  if (get_machine_state() >= MS_MUDLIB_LIMBO)
    {
      opt_trace (TT_COMPILE|3, "calling master apply: valid_object() for: \"%s\"", name);
      push_object (ob);
      mret = apply_master_ob (APPLY_VALID_OBJECT, 1);
      if (mret && !MASTER_APPROVED (mret))
        {
          destruct_object (ob);
          error ("*master::%s() denied permission to load '/%s'.", APPLY_VALID_OBJECT, name);
        }
    }

  if (init_object (ob))
    {
      opt_trace (TT_COMPILE|3, "calling object create(): \"%s\"", name);
      call_create (ob, 0);
    }

  if (!(ob->flags & O_DESTRUCTED) && function_exists (APPLY_CLEAN_UP, ob, 1))
    {
      ob->flags |= O_WILL_CLEAN_UP;
    }
  command_giver = save_command_giver;
  ob->load_time = current_time;
  num_objects_this_thread--;
  return ob;
}


/**
 * @brief Create a new name for a cloned object by appending #number to the original name.
 * 
 * The number is incremented with each call to this function.
 * @param str The original object name.
 * @return A new string with the cloned object name.
 */
static char *make_new_name (const char *str) {
  static int i = 1;
  char *p = DXALLOC (strlen (str) + 10, TAG_OBJ_NAME, "make_new_name");

  (void) sprintf (p, "%s#%d", str, i);
  i++;
  return p;
}


/*
 * Save the command_giver, because reset() in the new object might change
 * it.
 */
object_t *clone_object (const char *str1, int num_arg) {

  object_t *ob, *new_ob;
  object_t *save_command_giver = command_giver;

  if (current_object && current_object->euid == 0)
    {
      if (current_object != master_ob)
        error ("*Attempt to create object without effective UID.");
    }
  num_objects_this_thread = 0;
  ob = find_or_load_object (str1);
  if (ob && !object_visible (ob))
    ob = 0;
  /*
   * If the object self-destructed...
   */
  if (ob == 0)
    {				/* fix from 3.1.1 */
      pop_n_elems (num_arg);
      return (0);
    }
  if (ob->flags & O_CLONE)
    {
      if (!(ob->flags & O_VIRTUAL) || strrchr (str1, '#'))
        error ("*Cannot clone from a clone!");
      else
        {
          /*
           * well... it's a virtual object.  So now we're going to "clone"
           * it.
           */
          svalue_t *v;

          pop_n_elems (num_arg);	/* possibly this should be smarter */
          /* but then, this whole section is a
             kludge and should be looked at.

             Note that create() never gets called
             in clones of virtual objects.
             -Beek */

          if (!(str1 = strip_and_check_name (str1)))
            error ("*Filenames with consecutive /'s in them aren't allowed (%s).", str1);

          if (ob->ref == 1 && !ob->super && !ob->contains)
            {
              /*
               * ob unused so reuse it instead to save space. (possibly
               * loaded just for cloning)
               */
              new_ob = ob;
            }
          else
            {
              /* can't reuse, so load another */
              if (!(v = load_virtual_object (str1)))
                return 0;
              new_ob = v->u.ob;
            }
          remove_object_hash (new_ob);
          if (new_ob->name)
            FREE (new_ob->name);
          /* Now set the file name of the specified object correctly... */
          new_ob->name = make_new_name (str1);
          enter_object_hash (new_ob);
          new_ob->flags |= O_VIRTUAL;
          new_ob->load_time = current_time;
          command_giver = save_command_giver;
          return (new_ob);
          /*
           * we can skip all of the stuff below since we were already
           * cloned once to have gotten to this stage.
           */
        }
    }

  /* We do not want the heart beat to be running for unused copied objects */
  if (ob->flags & O_HEART_BEAT)
    (void) set_heart_beat (ob, 0);
  new_ob = get_empty_object (ob->prog->num_variables_total);
  new_ob->name = make_new_name (ob->name);
  opt_trace (TT_MEMORY|3, "clone object name: \"/%s\"", new_ob->name);
  new_ob->flags |= (O_CLONE | (ob->flags & (O_WILL_CLEAN_UP | O_WILL_RESET)));
  new_ob->load_time = ob->load_time;
  new_ob->prog = ob->prog;
  reference_prog (ob->prog, "clone_object");
  DEBUG_CHECK (!current_object, "clone_object() from no current_object !\n");

  init_object (new_ob);

  new_ob->next_all = obj_list;
  obj_list = new_ob;
  opt_info (1, "cloning object /%s", obj_list->name);
  enter_object_hash (new_ob);	/* Add name to fast object lookup table */
  call_create (new_ob, num_arg);
  command_giver = save_command_giver;
  /* Never know what can happen ! :-( */
  if (new_ob->flags & O_DESTRUCTED)
    return (0);
  return (new_ob);
}

object_t* environment (svalue_t * arg) {
  object_t *ob = current_object;

  if (arg && arg->type == T_OBJECT)
    ob = arg->u.ob;
  if (ob == 0 || ob->super == 0 || (ob->flags & O_DESTRUCTED))
    return 0;
  if (ob->flags & O_DESTRUCTED)
    error ("*environment() of destructed object.");
  return ob->super;
}

/*
 * Take a user command and parse it.
 * The command can also come from a NPC.
 * Beware that 'str' can be modified and extended !
 */
int process_command (char *str, object_t * ob) {
  object_t *save = command_giver;
  int res;

  /* disallow users to issue commands containing ansi escape codes */
#if defined(NO_ANSI) && !defined(STRIP_BEFORE_PROCESS_INPUT)
  char *c;

  for (c = str; *c; c++)
    {
      if (*c == 27)
        {
          *c = ' ';		/* replace ESC with ' ' */
        }
    }
#endif
  command_giver = ob;
  res = user_parser (str);
  command_giver = save;
  return (res);
}				/* process_command() */

/**
 * Execute a command for an object.
 * Copy the command into a new buffer, because 'process_command()' can modify the command.
 * If the object is not current object, static functions will not * be executed.
 * This will prevent forcing users to do illegal things.
 *
 * Return cost of the command executed if success (> 0).
 * When failure, return 0.
 */
int64_t command_for_object (char *str) {

  char buff[1000];
  int64_t save_eval_cost = eval_cost;

  if (strlen (str) > sizeof (buff) - 1)
    error ("*Too long command.");
  else if (current_object->flags & O_DESTRUCTED)
    return 0;
  strncpy (buff, str, sizeof buff);
  buff[sizeof buff - 1] = '\0';
  if (process_command (buff, current_object))
    return save_eval_cost - eval_cost;
  else
    return 0;
}

/*
 * With no argument, present() looks in the inventory of the current_object,
 * the inventory of our super, and our super.
 * If the second argument is nonzero, only the inventory of that object
 * is searched.
 */


static object_t *object_present2 (char *, object_t *);

object_t* object_present (svalue_t * v, object_t * ob) {
  svalue_t *ret;
  object_t *ret_ob;
  int specific = 0;

  if (ob == 0)
    ob = current_object;
  else
    specific = 1;

  if (ob->flags & O_DESTRUCTED)
    return 0;

  if (v->type == T_OBJECT)
    {
      if (specific)
        {
          if (v->u.ob->super == ob)
            return v->u.ob;
          else
            return 0;
        }

      if (v->u.ob->super == ob ||
          (v->u.ob->super == ob->super && ob->super != 0))
        return v->u.ob->super;

      return 0;
    }

  ret_ob = object_present2 (v->u.string, ob->contains);

  if (ret_ob)
    return ret_ob;

  if (specific)
    return 0;

  if (ob->super)
    {
      push_svalue (v);
      ret = apply (APPLY_ID, ob->super, 1, ORIGIN_DRIVER);

      if (ob->super->flags & O_DESTRUCTED)
        return 0;

      if (!IS_ZERO (ret))
        return ob->super;

      return object_present2 (v->u.string, ob->super->contains);
    }

  return 0;
}

/**
 * Help function for object_present().
 * Looks for an object named 'str' in the inventory 'ob'.
 * An optional number following the object name indicates which one to find.
 * For example, "sword 2" finds the second sword in the inventory.
 * @param str The name of the object to find, possibly with a number suffix.
 * @param ob The inventory to search in.
 * @return The found object, or NULL if not found.
 */
static object_t* object_present2 (char *str, object_t * ob) {

  svalue_t *ret;
  char *p;
  size_t count = 0, length;

  if ((length = strlen (str)))
    {
      p = str + length - 1;
      if (isdigit (*p))
        {
          do
            {
              p--;
            }
          while (p > str && isdigit (*p));

          if (*p == ' ')
            {
              count = atoi (p + 1) - 1;
              length = p - str;
            }
        }
    }

  for (; ob; ob = ob->next_inv)
    {
      p = new_string (length, "object_present2");
      memcpy (p, str, length);
      p[length] = 0;

      push_malloced_string (p);
      ret = apply (APPLY_ID, ob, 1, ORIGIN_DRIVER);

      if (ob->flags & O_DESTRUCTED)
        return 0;

      if (IS_ZERO (ret))
        continue;

      if (count-- > 0)
        continue;

      return ob;
    }

  return 0;
}

/**
 * @brief Initialize the master object.
 * 
 * If error occurs, print error messages to standard error and exit with failure.
 * @param master_file The path to the master object file.
 */
void init_master (const char *master_file) {
  char buf[PATH_MAX];
  object_t *new_ob;

  if (!master_file || !master_file[0])
    {
      /* If master file was not specified correctly, we don't expect the logger would
       * work either, so print to stderr instead.
       */
      fprintf (stderr, "No master object specified in config file.\n");
      exit (-1);
    }
  opt_info(1, "Loading master object: %s\n", master_file);

  if (!strip_name (master_file, buf, sizeof (buf)))
    {
      fprintf (stderr, "Illegal master file name '%s'", master_file);
      exit(-1);
    }

  if (master_file[strlen (master_file) - 2] != '.')
    strncat (buf, ".c", sizeof(buf) - strlen(buf) - 1);

  new_ob = load_object (buf, 0);
  if (new_ob == 0)
    {
      fprintf (stderr, "The master file %s was not loaded.\n", master_file);
      exit (-1);
    }
  set_master (new_ob);
}

static char *saved_master_name = "";
static char *saved_simul_name = "";

static void fix_object_names (void) {
  master_ob->name = saved_master_name;
  simul_efun_ob->name = saved_simul_name;
}

static object_t *restrict_destruct;

void reset_destruct_object_limits() {
  restrict_destruct = NULL;
}

/**
 * Remove an object. It is first moved into the \c ob_list_destruct linked
 * list, and not really deallocated until later. (see destruct2()).
 * @param ob The object to destruct.
 */
void destruct_object (object_t * ob) {
  object_t **pp;
  int removed;
  object_t *super;
  object_t *save_restrict_destruct = restrict_destruct;

  DEBUG_CHECK (!ob, "destruct_object() called with NULL pointer.\n");

  opt_trace (TT_EVAL|1, "start destructing: /%s", ob->name);
  if (restrict_destruct && restrict_destruct != ob)
    error ("*Only this_object() can be destructed from move_or_destruct.");

  if ((ob == simul_efun_ob) && master_ob)
    {
      /* simul efun object is a special object that the F_SIMUL_EFUN instruction in compiled LPC
       * opcodes relies on the correct associations of simul_num and the function defined in
       * simul_efun_ob. If both master_object and simul_efun_object exist, then
       * destructing simul_efun_object would break this association and possibly corrupt the
       * program of master object.
       * 
       * In Neolith, we allow the simul_efun_object to be destructed only when the
       * master_object does not exist, such as during mudlib reloading. More validations are
       * checked in the set_simul_efun() function when replacing the simul_efun_ob with a new object.
       */
      error ("*Cannot destruct simul_efun_object while master_object exists.");
    }

  /*
   * check if object has an efun socket referencing it for a callback. if
   * so, close the efun socket.
   */
  if (ob->flags & O_EFUN_SOCKET)
    {
      close_referencing_sockets (ob);
    }
#ifdef PACKAGE_PARSER
  if (ob->pinfo)
    {
      parse_free (ob->pinfo);
      ob->pinfo = 0;
    }
#endif

  if (ob->flags & O_DESTRUCTED)
    {
      opt_trace (TT_EVAL|1, "object /%s already destructed", ob->name);
      return;
    }

  remove_object_from_stack (ob);
  if (apply_ret_value.type == T_OBJECT && apply_ret_value.u.ob == ob)
    {
      /* clear apply_ret_value if it references the destructed object */
      free_svalue (&apply_ret_value, "destruct_object");
      apply_ret_value = const0;
    }

  /* try to move our contents somewhere */
  super = ob->super;

  while (ob->contains)
    {
      object_t *otmp = ob->contains;
      /*
      * An error here will not leave destruct() in an inconsistent
      * stage.
      */
      if (g_proceeding_shutdown) /* if we are shutting down, don't move objects */
        {
          move_object (otmp, NULL);
          destruct_object (otmp);
          continue;
        }
      else
        {
          if (super && !(super->flags & O_DESTRUCTED))
            push_object (super);
          else
            push_number (0);

          restrict_destruct = ob->contains;
          (void) apply (APPLY_MOVE, ob->contains, 1, ORIGIN_DRIVER);
          restrict_destruct = save_restrict_destruct;

          /* OUCH! we could be dested by this. -Beek */
          if (ob->flags & O_DESTRUCTED)
            return;
        }

      if (otmp == ob->contains) /* not moved elsewhere ... see move_or_destruct() apply */
        destruct_object (otmp);
    }

#ifdef OLD_ED
  if (ob->interactive && ob->interactive->ed_buffer)
    save_ed_buffer (ob);
#else
  if (ob->flags & O_IN_EDIT)
    {
      object_save_ed_buffer (ob);
      ob->flags &= ~O_IN_EDIT;
    }
#endif

  if (ob->super)
    {
      /*
      * Remove us out of this current room (if any). Remove all sentences
      * defined by this object from all objects here.
      */
      if (ob->super)
        {
          if (ob->super->flags & O_ENABLE_COMMANDS)
            remove_sent (ob, ob->super);

          for (pp = &ob->super->contains; *pp;)
            {
              if ((*pp)->flags & O_ENABLE_COMMANDS)
                remove_sent (ob, *pp);

              if (*pp != ob)
                pp = &(*pp)->next_inv;
              else
                *pp = (*pp)->next_inv;
            }
        }
      opt_trace (TT_EVAL|1, "moved /%s out of environment", ob->name);
    }

  /* At this point, we can still back out, but this is the very last
   * minute we can do so.  Make sure we have a new object to replace
   * us if this is a vital object.
   */
  if ((ob == master_ob) || (ob == simul_efun_ob))
    {
      object_t *new_ob = NULL;
      char *tmp = ob->name; /* a shared string */
      char *vital_obj_name = NULL;

      /* this could be called before set_master() or set_simul_efun() returns */
      (++sp)->type = T_ERROR_HANDLER;
      sp->u.error_handler = fix_object_names;
      saved_master_name = master_ob ? master_ob->name : "";
      saved_simul_name = simul_efun_ob ? simul_efun_ob->name : "";

      /* hack to make sure we don't find ourselves at several points
         in the following process */
      ob->name = "";

      /* get current setting of the vital object name */
      if (ob == master_ob)
        vital_obj_name = CONFIG_STR (__MASTER_FILE__);
      else if (ob == simul_efun_ob)
        vital_obj_name = CONFIG_STR (__SIMUL_EFUN_FILE__);

      if (vital_obj_name && !g_proceeding_shutdown)
        {
          /* reload vital object */
          char new_name[PATH_MAX];
          if (!strip_name (vital_obj_name, new_name, sizeof (new_name)))
            {
              ob->name = tmp;
              sp--;
              error ("*Destruction of vital object rejected due to invalid config setting (\"%s\").", vital_obj_name);
            }
          opt_trace (TT_EVAL|1, "reloading vital object: /%s", tmp);
          new_ob = load_object (tmp, 0);
          if (!new_ob)
            {
              ob->name = tmp;
              sp--;
              error ("*Destruct on vital object failed: new copy failed to reload.");
            }
        }

      if (ob == master_ob)
        {
          set_master (new_ob); /* could be NULL */
        }
      else if (ob == simul_efun_ob)
        {
          set_simul_efun (new_ob); /* could be NULL */
        }

      sp--;			/* error handler */

      /* Set the name back so we can remove it from the hash table.
         Also be careful not to remove the new object, which has
         the same name. */
      ob->name = tmp;
      if (new_ob)
        {
          tmp = new_ob->name; /* could be different */
          new_ob->name = "";
        }
      remove_object_hash (ob);
      if (new_ob)
        new_ob->name = tmp;
    }
  else
    remove_object_hash (ob); /* not vital object */
  opt_trace (TT_EVAL|1, "removed /%s from object name hash table", ob->name);

  /*
   * Now remove us out of the list of all objects. This must be done last,
   * because an error in the above code would halt execution.
   */
  removed = 0;
  for (pp = &obj_list; *pp; pp = &(*pp)->next_all)
    {
      if (*pp != ob)
        continue;
      *pp = (*pp)->next_all;
      removed = 1;
      break;
    }
  if (!removed)
    debug_error ("Failed to remove object %s from all objects list.", ob->name);

  if (ob->living_name)
    {
      opt_trace (TT_EVAL|1, "removing /%s (%s) from living objects list", ob->name, ob->living_name);
      remove_living_name (ob);
    }

  /* Free all sentences defined by this object (actions it can respond to).
   * This must be done before deallocation to properly release references
   * to other objects. Without this, cross-referencing objects (e.g., NPCs
   * that add_action to each other) will trigger ref count warnings. */
  if (ob->sent)
    {
      sentence_t *s, *next;
      opt_trace (TT_EVAL|1, "freeing sentences for /%s", ob->name);
      for (s = ob->sent; s; s = next)
        {
          next = s->next;
          free_sentence (s);
        }
      ob->sent = NULL;
    }

  /* Clean up any input_to references pointing to this object.
   * Without this, destructed objects remain in memory if users with pending
   * input_to prompts go AFK before typing anything. */
  if (all_users)
    {
      for (int i = 0; i < max_users; i++)
        {
          interactive_t *ip = all_users[i];
          if (ip && ip->input_to && ip->input_to->ob == ob)
            {
              opt_trace (TT_EVAL|1, "clearing input_to for /%s from user /%s",
                         ob->name, ip->ob->name);
              free_sentence (ip->input_to);
              ip->input_to = 0;

              /* Clear single-char mode if it was set for this input_to */
              if (ip->iflags & SINGLE_CHAR)
                {
                  ip->iflags &= ~SINGLE_CHAR;
                  /* Note: We don't call set_telnet_single_char() here because:
                   * 1. It's static in comm.c and would require API changes
                   * 2. The flag will be properly reset on next input_to/get_char call
                   * 3. User will revert to line mode on next input naturally */
                }
            }
        }
    }

  ob->flags &= ~O_ENABLE_COMMANDS;
  ob->super = 0;
  ob->next_inv = 0;
  ob->contains = 0;
  ob->next_all = obj_list_destruct;
  obj_list_destruct = ob;

  set_heart_beat (ob, 0);
  ob->flags |= O_DESTRUCTED; /* mark as destructed */

  /* moved this here from destruct2() -- see comments in destruct2() */
  if (ob->interactive)
    {
      opt_trace (TT_COMM|1, "disconnecting /%s as interactive object", ob->name);
      remove_interactive (ob, 1);
    }
  opt_trace (TT_EVAL|1, "finished destructing: /%s", ob->name);
}


/*
 * This one is called when no program is executing from the main loop.
 */
void destruct2 (object_t * ob) {
  /*
   * We must deallocate variables here, not in 'free_object()'. That is
   * because one of the local variables may point to this object, and
   * deallocation of this pointer will also decrease the reference count of
   * this object. Otherwise, an object with a variable pointing to itself,
   * would never be freed. Just in case the program in this object would
   * continue to execute, change string and object variables into the
   * number 0.
   */
  if (ob->prog->num_variables_total > 0)
    {
      /*
       * Deallocate variables in this object. The space of the variables
       * are not deallocated until the object structure is freed in
       * free_object().
       */
      int i;

      for (i = 0; i < (int) ob->prog->num_variables_total; i++)
        {
          free_svalue (&ob->variables[i], "destruct2");
          ob->variables[i] = const0u;
        }
    }
  if (ob->ref > 1)
    opt_warn (1, "object /%s has ref count %d\n", ob->name, ob->ref);

  /*
   * This decrements the reference count of the object. If the object is referenced
   * by other objects, it will not be freed until the last reference is gone. If the
   * object is not referenced by any other objects, it will be freed immediately.
   */
  free_object (ob, "destruct_object");
}

/* All destructed objects are moved into a sperate linked list,
 * and deallocated after program execution.  */

void remove_destructed_objects () {
  object_t *ob, *next;

  if (obj_list_replace)
    replace_programs ();
  for (ob = obj_list_destruct; ob; ob = next)
    {
      next = ob->next_all;
      destruct2 (ob);
    }
  obj_list_destruct = 0;
}				/* remove_destructed_objects() */

/*
 * say() efun - send a message to:
 *  all objects in the inventory of the source,
 *  all objects in the same environment as the source,
 *  and the object surrounding the source.
 *
 * when there is no command_giver, current_object is used as the source,
 *  otherwise, command_giver is used.
 *
 * message never goes to objects in the avoid array, or the source itself.
 *
 * rewritten, bobf@metronet.com (Blackthorn) 9/6/93
 */

static void send_say (object_t * ob, char *text, array_t * avoid) {
  int valid, j;

  for (valid = 1, j = 0; j < avoid->size; j++)
    {
      if (avoid->item[j].type != T_OBJECT)
        continue;
      if (avoid->item[j].u.ob == ob)
        {
          valid = 0;
          break;
        }
    }

  if (!valid)
    return;

  tell_object (ob, text);
}

void say (svalue_t * v, array_t * avoid) {
  object_t *ob, *origin, *save_command_giver = command_giver;
  char *buff;

  check_legal_string (v->u.string);
  buff = v->u.string;

  if (current_object->flags & O_LISTENER || current_object->interactive)
    command_giver = current_object;
  if (command_giver)
    origin = command_giver;
  else
    origin = current_object;

  /* To our surrounding object... */
  if ((ob = origin->super))
    {
      if (ob->flags & O_LISTENER || ob->interactive)
        send_say (ob, buff, avoid);

      /* And its inventory... */
      for (ob = origin->super->contains; ob; ob = ob->next_inv)
        {
          if (ob != origin && (ob->flags & O_LISTENER || ob->interactive))
            {
              send_say (ob, buff, avoid);
              if (ob->flags & O_DESTRUCTED)
                break;
            }
        }
    }
  /* Our inventory... */
  for (ob = origin->contains; ob; ob = ob->next_inv)
    {
      if (ob->flags & O_LISTENER || ob->interactive)
        {
          send_say (ob, buff, avoid);
          if (ob->flags & O_DESTRUCTED)
            break;
        }
    }

  command_giver = save_command_giver;
}

/*
 * Sends a string to all objects inside of a specific object.
 * Revised, bobf@metronet.com 9/6/93
 */
#ifdef F_TELL_ROOM
void tell_room (object_t * room, svalue_t * v, array_t * avoid) {
  object_t *ob;
  char *buff;
  int valid, j;
  char txt_buf[LARGEST_PRINTABLE_STRING];

  switch (v->type)
    {
    case T_STRING:
      check_legal_string (v->u.string);
      buff = v->u.string;
      break;
    case T_OBJECT:
      buff = v->u.ob->name;
      break;
    case T_NUMBER:
      buff = txt_buf;
      sprintf (buff, "%" PRId64, v->u.number);
      break;
    case T_REAL:
      buff = txt_buf;
      sprintf (buff, "%g", v->u.real);
      break;
    default:
      bad_argument (v, T_OBJECT | T_NUMBER | T_REAL | T_STRING,
                    2, F_TELL_ROOM);
      IF_DEBUG (buff = 0);
      return;
    }

  for (ob = room->contains; ob; ob = ob->next_inv)
    {
      if (!ob->interactive && !(ob->flags & O_LISTENER))
        continue;

      for (valid = 1, j = 0; j < avoid->size; j++)
        {
          if (avoid->item[j].type != T_OBJECT)
            continue;
          if (avoid->item[j].u.ob == ob)
            {
              valid = 0;
              break;
            }
        }

      if (!valid)
        continue;

      if (!ob->interactive)
        {
          tell_npc (ob, buff);
          if (ob->flags & O_DESTRUCTED)
            break;
        }
      else
        {
          tell_object (ob, buff);
          if (ob->flags & O_DESTRUCTED)
            break;
        }
    }
}
#endif

void shout_string (char *str) {
  object_t *ob;

  check_legal_string (str);

  for (ob = obj_list; ob; ob = ob->next_all)
    {
      if (!(ob->flags & O_LISTENER) || (ob == command_giver) || !ob->super)
        continue;
      tell_object (ob, str);
    }
}

/**
 *  @brief This will enable an object to use commands normally only
 *      accessible by interactive users.
 *      Also check if the user is a wizard. Wizards must not affect the
 *      value of the wizlist ranking.
 *  @param num If non-zero, enable commands, else disable commands.
 */

void enable_commands (int num) {
  if (current_object->flags & O_DESTRUCTED)
    return;

  if (num)
    {
      current_object->flags |= O_ENABLE_COMMANDS;
      command_giver = current_object;
      return;
    }

  if (!(current_object->flags & O_ENABLE_COMMANDS))
    return;

  current_object->flags &= ~O_ENABLE_COMMANDS;
  command_giver = 0;
}

/**
 * Set up a function in this object to be called with the next input string of current
 * command_giver.
 * 
 * @param fun The function to call, either as a string (function name) or a function pointer.
 * @param flag If I_SINGLE_CHAR is set, the function will be called with the next single
 *    character input instead of a whole line.
 * @param num_arg The number of additional arguments to pass to the function when called.
 * @param args The array of additional arguments to pass to the function.
 * @return 1 if the input_to was successfully set up, 0 if it failed (e.g., if command_giver
 *    is invalid or already has an input_to). If more than one input_to() is called before
 *    the first one is triggered, only the first call will succeed, and subsequent calls will
 *    return 0 but not raise an error.
 */
int input_to (svalue_t * fun, int flag, int num_arg, svalue_t * args) {

  sentence_t *s;
  funptr_t *callback_funp = 0;

  if (!command_giver || command_giver->flags & O_DESTRUCTED)
    return 0;

  s = alloc_sentence ();
  if (!set_call (command_giver, s, flag & ~I_SINGLE_CHAR))
    {
      /* LPC spec. says if input_to() is called more than once, only the first call succeeds.
       * No error is raised for subsequent calls, but the sentence created for the subsequent
       * call should be freed to avoid memory leaks.
       */
      free_sentence (s);
      return 0;
    }

  /* Convert string to function pointer or use existing funptr */
  if (fun->type == T_STRING)
    {
      /* Find function in current_object and create FP_LOCAL function pointer */
      svalue_t dummy;
      dummy.type = T_NUMBER;
      dummy.u.number = 0;
      opt_trace (TT_COMM|2, "set callback function to '%s' in object /%s", fun->u.string, current_object->name);
      callback_funp = make_lfun_funp_by_name (fun->u.string, &dummy); /* ref = 1, by sentence->function.f */
      if (!callback_funp)
        {
          error ("Function '%s' not found in input_to", fun->u.string);
        }
    }
  else if (fun->type == T_FUNCTION)
    {
      callback_funp = fun->u.fp;
      callback_funp->hdr.ref++; /* by sentence->function.f */
    }
  else
    {
      free_sentence (s);
      error ("input_to: fun must be string or function");
    }

  /* Store function pointer (always use V_FUNCTION now) */
  s->function.f = callback_funp;
  s->flags = V_FUNCTION;
  s->ob = 0; /* if callback is T_STRING, callback_funp already holds reference to the current_object */

  /* Store carryover args in SENTENCE (not interactive_t) */
  if (num_arg > 0)
    {
      array_t *arg_array = allocate_empty_array (num_arg); /* ref = 1 by sentence->args */
      for (int i = 0; i < num_arg; i++)
        assign_svalue_no_free (&arg_array->item[i], &args[i]);
      s->args = arg_array;
    }
  else
    {
      s->args = NULL;
    }

  return 1;
}


/**
 * Set up a function in this object to be called with the next
 * user input character.
 */
int get_char (svalue_t * fun, int flag, int num_arg, svalue_t * args) {
  sentence_t *s;
  funptr_t *callback_funp = 0;

  if (!command_giver || command_giver->flags & O_DESTRUCTED)
    return 0;

  s = alloc_sentence ();
  if (!set_call (command_giver, s, flag | I_SINGLE_CHAR))
    {
      /* LPC spec. says if get_char() is called more than once, only the first call succeeds.
       * No error is raised for subsequent calls, but the sentence created for the subsequent
       * call should be freed to avoid memory leaks.
       */
      free_sentence (s);
      return 0;
    }

  /* Convert string to function pointer or use existing funptr */
  if (fun->type == T_STRING)
    {
      /* Find function in current_object and create FP_LOCAL function pointer */
      svalue_t dummy;
      dummy.type = T_NUMBER;
      dummy.u.number = 0;
      opt_trace (TT_COMM|2, "set callback function to '%s' in object /%s", fun->u.string, current_object->name);
      callback_funp = make_lfun_funp_by_name (fun->u.string, &dummy);
      if (!callback_funp)
        {
          error ("Function '%s' not found in get_char", fun->u.string);
        }
    }
  else if (fun->type == T_FUNCTION)
    {
      callback_funp = fun->u.fp;
      callback_funp->hdr.ref++;
    }
  else
    {
      free_sentence (s);
      error ("get_char: fun must be string or function");
    }

  /* Store function pointer (always use V_FUNCTION now) */
  s->function.f = callback_funp;
  s->flags = V_FUNCTION;
  s->ob = 0; /* if callback is T_STRING, callback_funp already holds reference to the current_object */

  /* Store carryover args in SENTENCE (not interactive_t) */
  if (num_arg > 0)
    {
      array_t *arg_array = allocate_empty_array (num_arg);
      for (int i = 0; i < num_arg; i++)
        assign_svalue_no_free (&arg_array->item[i], &args[i]);
      s->args = arg_array; /* ref = 1 by allocate_empty_array */
    }
  else
    {
      s->args = NULL;
    }

  return 1;
}

void print_svalue (svalue_t * arg) {
  char tbuf[2048];

  if (arg == 0)
    {
      tell_object (command_giver, "<NULL>");
    }
  else
    switch (arg->type)
      {
      case T_STRING:
        check_legal_string (arg->u.string);
        tell_object (command_giver, arg->u.string);
        break;
      case T_OBJECT:
        sprintf (tbuf, "OBJ(/%s)", arg->u.ob->name);
        tell_object (command_giver, tbuf);
        break;
      case T_NUMBER:
        sprintf (tbuf, "%" PRId64, arg->u.number);
        tell_object (command_giver, tbuf);
        break;
      case T_REAL:
        sprintf (tbuf, "%g", arg->u.real);
        tell_object (command_giver, tbuf);
        break;
      case T_ARRAY:
        tell_object (command_giver, "<ARRAY>");
        break;
      case T_MAPPING:
        tell_object (command_giver, "<MAPPING>");
        break;
      case T_FUNCTION:
        tell_object (command_giver, "<FUNCTION>");
        break;
      case T_BUFFER:
        tell_object (command_giver, "<BUFFER>");
        break;
      default:
        tell_object (command_giver, "<UNKNOWN>");
        break;
      }
  return;
}


void do_write (svalue_t * arg) {
  object_t *save_command_giver = command_giver;

  if (!command_giver)
    command_giver = current_object;
  print_svalue (arg);
  command_giver = save_command_giver;
}

/**
 *  @brief Find an object. If not loaded, load it !
 *  The object may self-destruct, which is the only case when 0 will be returned.
 *  This was called find_object() before. Neolith renamed it to avoid confusion
 *  with the efun of the same name.
 *  @param str The name of the object to find or load.
 *  @return The object found or loaded, or 0 if it could not be found/loaded.
 */

object_t *find_or_load_object (const char *str) {

  object_t *ob;
  char tmpbuf[MAX_OBJECT_NAME_SIZE];

  if (!strip_name (str, tmpbuf, sizeof tmpbuf))
    return 0;

  if ((ob = lookup_object_hash (tmpbuf)))
    return ob;

  ob = load_object (tmpbuf, 0);
  if (!ob || (ob->flags & O_DESTRUCTED))	/* *sigh* */
    return 0;

  return ob;
}

/**
 *  @brief Look for a loaded object. Return 0 if non found.
 *  This was called find_object2() before. Neolith renamed it to avoid confusion
 *  with the efun of the same name.
 *  @param str The name of the object to find.
 *  @return The object found or 0 if not found.
 */
object_t *find_object_by_name (const char *str) {

  register object_t *ob;
  char p[MAX_OBJECT_NAME_SIZE];

  if (!strip_name (str, p, sizeof p))
    return 0;

  if ((ob = lookup_object_hash (p)))
    return ob;

  return 0;
}

/**
 * Transfer an object.
 * The object has to be taken from one inventory list and added to another.
 * The main work is to update all command definitions, depending on what is
 * living or not. Note that all objects in the same inventory are affected.
 *
 * @param item The object to move.
 * @param dest The destination object to move to. If NULL, the object will
 *    be moved to the top level (no environment). This is a Neolith extension.
 * @note Recursive moves (moving an object inside itself) are not allowed
 *    and will raise an error. Moving to a destructed object is also not
 *    allowed and will raise an error.
 * @note The init() function of the destination object and all present objects
 *    will be called after the move, which may cause further moves or even
 *    destruction. The function will check for such changes and handle them
 *    properly to avoid crashes or inconsistent states.
 */
void move_object (object_t * item, object_t * dest) {
  object_t **pp, *ob;
  object_t *next_ob;
  object_t *save_cmd = command_giver;

  /* Recursive moves are not allowed. */
  for (ob = dest; ob; ob = ob->super)
    if (ob == item)
      error ("*Can't move object inside itself.");

  if (dest && dest->flags & O_DESTRUCTED)
    error ("*Can't move to a destructed object.");

#ifdef LAZY_RESETS
  if (dest)
    try_reset (dest);
#endif

  if (item->super)
    {
      if (item->flags & O_ENABLE_COMMANDS)
        remove_sent (item->super, item);

      if (item->super->flags & O_ENABLE_COMMANDS)
        remove_sent (item, item->super);

      for (pp = &item->super->contains; *pp;)
        {
          if (*pp != item)
            {
              if ((*pp)->flags & O_ENABLE_COMMANDS)
                remove_sent (item, *pp);
              if (item->flags & O_ENABLE_COMMANDS)
                remove_sent (*pp, item);
              pp = &(*pp)->next_inv;
              continue;
            }

          /* unlink object from original inventory list */
          *pp = item->next_inv;
        }
    }

  /* link object into target's inventory list */
  item->super = dest;
  if (dest)
    {
      item->next_inv = dest->contains;
      dest->contains = item;
    }
  else
    {
      item->next_inv = 0;
      return;
    }

  /*
   * Setup the new commands. The order is very important, as commands in
   * the room should override commands defined by the room. Beware that
   * init() in the room may have moved 'item' !
   * 
   * The call of init() should really be done by the object itself (except in
   * the -o mode). It might be too slow, though :-(
   */
  if (item->flags & O_ENABLE_COMMANDS)
    {
      command_giver = item;
      (void) apply (APPLY_INIT, dest, 0, ORIGIN_DRIVER);
      if ((dest->flags & O_DESTRUCTED) || item->super != dest)
        {
          command_giver = save_cmd;	/* marion */
          return;
        }
    }

  /*
   * Run init of the item once for every present user, and for the
   * environment (which can be a user).
   */
  for (ob = dest->contains; ob; ob = next_ob)
    {
      next_ob = ob->next_inv;
      if (ob == item)
        continue;

      if (ob->flags & O_DESTRUCTED)
        error ("*An object was destructed at call of " APPLY_INIT "()");

      if (ob->flags & O_ENABLE_COMMANDS)
        {
          command_giver = ob;
          (void) apply (APPLY_INIT, item, 0, ORIGIN_DRIVER);
          if (dest != item->super)
            {
              command_giver = save_cmd;	/* marion */
              return;
            }
        }

      if (item->flags & O_DESTRUCTED)	/* marion */
        error ("*The object to be moved was destructed at call of " APPLY_INIT "()!");

      if (item->flags & O_ENABLE_COMMANDS)
        {
          command_giver = item;
          (void) apply (APPLY_INIT, ob, 0, ORIGIN_DRIVER);
          if (dest != item->super)
            {
              command_giver = save_cmd;	/* marion */
              return;
            }
        }
    }

  if (dest->flags & O_DESTRUCTED)	/* marion */
    error ("*The destination to move to was destructed at call of " APPLY_INIT "()!");

  if (dest->flags & O_ENABLE_COMMANDS)
    {
      command_giver = dest;
      (void) apply (APPLY_INIT, item, 0, ORIGIN_DRIVER);
    }

  command_giver = save_cmd;
}


/*
 * Find the sentence for a command from the user.
 * Return success status.
 */

#define MAX_VERB_BUFF 100

int user_parser (char *buff) {
  char verb_buff[MAX_VERB_BUFF];
  sentence_t *s;
  char *p;
  ptrdiff_t length;
  object_t *save_command_giver = command_giver;
  char *user_verb = 0;
  int where;
  int save_illegal_sentence_action;

  for (p = buff + strlen (buff) - 1; p >= buff; p--)
    {
      if (isspace (*p))
        continue;
      *(p + 1) = '\0';		/* truncate */
      break;
    }

  if (buff[0] == '\0')		/* empty line ? */
    return 0;

  if (0 == (command_giver->flags & O_ENABLE_COMMANDS))
    return 0;

  length = p - buff + 1;
  p = strchr (buff, ' ');
  if (p == 0)
    {
      user_verb = findstring (buff);
    }
  else
    {
      *p = '\0';
      user_verb = findstring (buff);
      *p = ' ';
      length = p - buff;
    }

  if (!user_verb)
    {
      /* either an xverb or a verb without a specific add_action */
      user_verb = buff;
    }

  /*
   * copy user_verb into a static character buffer to be pointed to by
   * last_verb.
   */
  strncpy (verb_buff, user_verb, MAX_VERB_BUFF - 1);
  if (p)
    {
      ptrdiff_t pos;

      pos = p - buff;
      if (pos < MAX_VERB_BUFF)
        {
          verb_buff[pos] = '\0';
        }
    }

  save_illegal_sentence_action = illegal_sentence_action;
  illegal_sentence_action = 0;

  for (s = save_command_giver->sent; s; s = s->next)
    {
      svalue_t *ret;

      /* Skip sentences from destructed objects (ref counting keeps memory valid) */
      if (s->ob->flags & O_DESTRUCTED)
        continue;

      if (s->flags & (V_NOSPACE | V_SHORT))
        {
          if (strncmp (buff, s->verb, strlen (s->verb)) != 0)
            continue;
        }
      else
        {
          /* note: if was add_action(blah, "") then accept it */
          if (s->verb[0] && (user_verb != s->verb))
            continue;
        }

      if (s->flags & V_NOSPACE)
        {
          size_t l1 = strlen (s->verb);
          size_t l2 = strlen (verb_buff);

          if (l1 < l2)
            last_verb = verb_buff + l1;
          else
            last_verb = "";
        }
      else
        {
          if (!s->verb[0] || (s->flags & V_SHORT))
            last_verb = verb_buff;
          else
            last_verb = s->verb;
        }

      /*
       * If the function is static and not defined by current object, then
       * it will fail. If this is called directly from user input, then
       * the origin is the driver and it will be allowed.
       */
      where = (current_object ? ORIGIN_EFUN : ORIGIN_DRIVER);

      /* Push command args FIRST (correct LPC order) */
      if (s->flags & V_NOSPACE)
        copy_and_push_string (&buff[strlen (s->verb)]);
      else if (buff[length] == ' ')
        copy_and_push_string (&buff[length + 1]);
      else
        push_undefined ();

      /* Push carryover args AFTER command args */
      int num_args = 1;  /* Command args */
      if (s->args)
        {
          for (int i = 0; i < s->args->size; i++)
            {
              push_svalue (&s->args->item[i]);
            }
          num_args += s->args->size;
        }

      /* Call function with all args */
      if (s->flags & V_FUNCTION)
        ret = call_function_pointer (s->function.f, num_args);
      else
        {
          if (s->function.s[0] == APPLY___INIT_SPECIAL_CHAR)
            error ("*Illegal function name.");
          ret = apply (s->function.s, s->ob, num_args, where);
//        ret = apply (s->function.s, s->ob, 1, ORIGIN_DRIVER);
        }

      /* s may be dangling at this point */

      command_giver = save_command_giver;

      last_verb = 0;

      /* was this the right verb? */
      if (ret == 0)
        {
          /* is it still around?  Otherwise, ignore this ...
             it moved somewhere or dested itself */
          if (s == save_command_giver->sent)
            {
              if (s->flags & V_FUNCTION)
                {
                  error ("*Verb '%s' bound to uncallable function pointer.", s->verb);
                }
              else
                {
                  error ("*Function for verb '%s' not found.", s->verb);
                }
            }
        }

      if (ret && (ret->type != T_NUMBER || ret->u.number != 0))
        {
          if (!illegal_sentence_action)
            illegal_sentence_action = save_illegal_sentence_action;
          return 1;
        }

      if (illegal_sentence_action)
        {
          switch (illegal_sentence_action)
            {
            case 1:
              error ("*Illegal to call remove_action() from a verb returning zero.");
            case 2:
              error ("*Illegal to move or destruct an object defining actions from a verb function which returns zero.");
            }
        }
    }

  notify_no_command ();
  illegal_sentence_action = save_illegal_sentence_action;

  return 0;
}

/*
 * Associate a command with function in this object.
 *
 * The optinal third argument is a flag that will state that the verb should
 * only match against leading characters.
 *
 * The optional varargs after the flag are carryover arguments that will be
 * passed to the action function after the command argument.
 *
 * The object must be near the command giver, so that we ensure that the
 * sentence is removed when the command giver leaves.
 *
 * If the call is from a shadow, make it look like it is really from
 * the shadowed object.
 */
void add_action (svalue_t * str, char *cmd, int flag, int num_carry, svalue_t *carry_args) {
  sentence_t *p;
  object_t *ob;

  if (current_object->flags & O_DESTRUCTED)
    return;
  ob = current_object;
#ifndef NO_SHADOWS
  while (ob->shadowing)
    {
      ob = ob->shadowing;
    }
  /* don't allow add_actions of a static function from a shadowing object */
  if ((ob != current_object) && str->type == T_STRING
      && is_static (str->u.string, ob))
    {
      return;
    }
#endif
  if (command_giver == 0 || (command_giver->flags & O_DESTRUCTED))
    return;
  if (ob != command_giver
      && ob->super != command_giver &&
      ob->super != command_giver->super && ob != command_giver->super)
    return;			/* No need for an error, they know what they
                                 * did wrong. */
  p = alloc_sentence ();
  if (str->type == T_STRING)
    {
      p->function.s = make_shared_string (str->u.string);
      p->flags = flag;
    }
  else
    {
      p->function.f = str->u.fp;
      str->u.fp->hdr.ref++;
      p->flags = flag | V_FUNCTION;
    }
  p->ob = ob;
  add_ref (ob, "add_action");
  p->verb = make_shared_string (cmd);

  /* Store carryover args in sentence */
  if (num_carry > 0)
    {
      array_t *arg_array = allocate_empty_array (num_carry);
      for (int i = 0; i < num_carry; i++)
        assign_svalue_no_free (&arg_array->item[i], &carry_args[i]);
      p->args = arg_array;
    }
  else
    {
      p->args = NULL;
    }

  /* This is ok; adding to the top of the list doesn't harm anything */
  p->next = command_giver->sent;
  command_giver->sent = p;
}


/*
 * Remove sentence with specified verb and action.  Return 1
 * if success.  If command_giver, remove his action, otherwise
 * remove current_object's action.
 */
int remove_action (char *act, char *verb) {
  object_t *ob;
  sentence_t **s;

  if (command_giver)
    ob = command_giver;
  else
    ob = current_object;

  if (ob)
    {
      for (s = &ob->sent; *s; s = &((*s)->next))
        {
          sentence_t *tmp;

          if (((*s)->ob == current_object) && (!((*s)->flags & V_FUNCTION))
              && !strcmp ((*s)->function.s, act)
              && !strcmp ((*s)->verb, verb))
            {
              tmp = *s;
              *s = tmp->next;
              free_sentence (tmp);
              illegal_sentence_action = 1;
              return 1;
            }
        }
    }
  return 0;
}


/**
 * Remove all commands (sentences) defined by object 'ob' in object 'user'
 */
static void remove_sent (object_t * ob, object_t * user) {
  sentence_t **s;

  for (s = &user->sent; *s;)
    {
      sentence_t *tmp;

      if ((*s)->ob == ob)
        {
          tmp = *s;
          *s = tmp->next;
          free_sentence (tmp);
          illegal_sentence_action = 2;
        }
      else
        s = &((*s)->next);
    }
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

/*
 * Write out a trace. If there is a heart_beat(), then return the
 * object that had that heart beat.
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

/* fatal() - Fatal error handler
 *
 * Prints error message to debug log and exit program.
 * */
static int proceeding_fatal_error = 0;

int in_fatal_error() {
  return (proceeding_fatal_error != 0);
}

void fatal (char *fmt, ...) {
  char *msg = "(error message buffer cannot be allocated)";
  va_list args;

  va_start (args, fmt);
#ifdef _GNU_SOURCE
  if (-1 == vasprintf (&msg, fmt, args))
#else
  int len = _vscprintf (fmt, args) + 1;
  msg = (char *) DXALLOC (len, TAG_TEMPORARY, "fatal");
  if (-1 == vsnprintf (msg, len, fmt, args))
#endif
    {
      debug_message("{}\t***** failed to format fatal error message \"%s\".", fmt);
      exit (EXIT_FAILURE);
    }
  va_end (args);

  debug_message ("{}\t***** %s", msg);

  if (proceeding_fatal_error)
    {
      debug_message ("{}\t***** fatal error occured while another proceeding, shutdown immediately.");
    }
  else
    {
      char *ob_name;
      error_context_t econ;

      proceeding_fatal_error = 1;

      if (current_file)
        debug_message ("{}\t----- compiling %s at line %d", current_file, current_line);

      if (current_object)
        debug_message ("{}\t----- current object was /%s", current_object->name);

      if ((ob_name = dump_trace (DUMP_WITH_ARGS | DUMP_WITH_LOCALVARS)))
        debug_message ("{}\t----- in heart beat of /%s", ob_name);

      save_context (&econ);
      if (setjmp (econ.context))
        {
          restore_context (&econ);
          debug_message ("{}\t***** error in master::%s(), shutdown now.", APPLY_CRASH);
        }
      else
        {
          svalue_t* ret;

          copy_and_push_string (msg);

          if (command_giver)
            push_object (command_giver);
          else
            push_undefined ();

          if (current_object)
            push_object (current_object);
          else
            push_undefined ();

          ret = apply_master_ob (APPLY_CRASH, 3);
          if (ret && ret != (svalue_t*)-1)
            {
              debug_message ("{}\t----- mudlib crash handler finished, shutdown now.");
            }
        }
      pop_context (&econ);
    }

  free (msg);

  if (CONFIG_INT (__ENABLE_CRASH_DROP_CORE__))
    abort ();
  else
    exit (EXIT_FAILURE);
}

/**
 * In original LPMud/MudOS, this is called from the shutdown() efun and
 * exit the driver process in the middle of execution. This is a bad practice
 * if the driver will integrate with other services, so we separate the shutdown
 * logic from the efun call.
 *
 * In Neolith, the shutdown() efun raises the g_proceeding_shutdown flag, and the
 * backend loop will return to main() which call this function to perform the
 * actual shutdown.
 *
 * @return This function does not return. It exits the process and return the
 *         g_exit_code to the operating system.
 */
void do_shutdown () {

  int i;

  ipc_remove ();

  /* force close all LPC sockets if mudlib doesn't close them */
  for (i = 0; i < max_lpc_socks; i++)
    {
      if (lpc_socks[i].state != CLOSED)
        (void) SOCKET_CLOSE (lpc_socks[i].fd);
    }

  for (i = 1; i < max_users; i++)
    {
      if (!all_users[i])
        continue;
      if (!(all_users[i]->iflags & CLOSING))
        {
          flush_message (all_users[i]); /* flush any pending output before closing */
          (void) SOCKET_CLOSE (all_users[i]->fd);
        }
    }

  /* shutdown console worker if active */
  if (g_console_worker)
    {
      if (!console_worker_shutdown(g_console_worker, 5000))
        {
          debug_warn ("Console worker did not stop within timeout\n");
        }
      console_worker_destroy(g_console_worker);
      g_console_worker = NULL;
    }

  if (g_console_queue)
    {
      async_queue_destroy(g_console_queue);
      g_console_queue = NULL;
    }
      
  /* destroy async runtime */
  if (g_runtime)
    {
      async_runtime_deinit (g_runtime);
      g_runtime = NULL;
    }

  if (MAIN_OPTION(pedantic))
    {
      debug_message ("{}\ttearing down world simulation");
      tear_down_simulate();
      debug_message ("{}\tdeinitializing all subsystems");
      deinit_lpc_compiler();
      deinit_strings();
      deinit_config();
    }

#ifdef WINSOCK
  WSACleanup(); /* for graceful shutdown */
#endif

#ifdef PROFILING
  monitor (0, 0, 0, 0, 0);	/* cause gmon.out to be written */
#endif

  exit (g_exit_code);
}

/*
 * Call this one when there is only little memory left. It will start
 * Armageddon.
 */
void do_slow_shutdown (int minutes) {
  /*
   * Swap out objects, and free some memory.
   */
  svalue_t *amo;

  push_number (minutes);
  amo = apply_master_ob (APPLY_SLOW_SHUTDOWN, 1);
  /* in this case, approved means the mudlib will handle it */
  if (!MASTER_APPROVED (amo))
    {
      object_t *save_current = current_object, *save_command = command_giver;

      command_giver = 0;
      current_object = 0;
      shout_string ("Out of memory.\n");
      command_giver = save_command;
      current_object = save_current;
      g_proceeding_shutdown = 1;
      return;
    }
}


void do_message (svalue_t * msg_class, svalue_t * msg, array_t * scope, array_t * exclude, int recurse) {
  int i, j, valid;
  object_t *ob;

  for (i = 0; i < scope->size; i++)
    {
      switch (scope->item[i].type)
        {
        case T_STRING:
          ob = find_or_load_object (scope->item[i].u.string);
          if (!ob || !object_visible (ob))
            continue;
          break;
        case T_OBJECT:
          ob = scope->item[i].u.ob;
          break;
        default:
          continue;
        }
      if (ob->flags & O_LISTENER || ob->interactive)
        {
          for (valid = 1, j = 0; j < exclude->size; j++)
            {
              if (exclude->item[j].type != T_OBJECT)
                continue;
              if (exclude->item[j].u.ob == ob)
                {
                  valid = 0;
                  break;
                }
            }
          if (valid)
            {
              push_svalue (msg_class);
              push_svalue (msg);
              apply (APPLY_RECEIVE_MESSAGE, ob, 2, ORIGIN_DRIVER);
            }
        }
      else if (recurse)
        {
          array_t *tmp;

          tmp = all_inventory (ob, 1);
          do_message (msg_class, msg, tmp, exclude, 0);
          free_array (tmp);
        }
    }
}


#ifdef LAZY_RESETS
void try_reset (object_t * ob) {
  if ((ob->next_reset < current_time) && !(ob->flags & O_RESET_STATE))
    {
      /* need to set the flag here to prevent infinite loops in apply_low */
      ob->flags |= O_RESET_STATE;
      reset_object (ob);
    }
}
#endif


#ifdef F_FIRST_INVENTORY
object_t* first_inventory (svalue_t * arg) {
  object_t *ob;

  if (arg->type == T_STRING)
    {
      ob = find_or_load_object (arg->u.string);
      if (ob && !object_visible (ob))
        ob = 0;
    }
  else
    ob = arg->u.ob;
  if (ob == 0)
    bad_argument (arg, T_STRING | T_OBJECT, 1, F_FIRST_INVENTORY);
  ob = ob->contains;
  while (ob)
    {
      if (ob->flags & O_HIDDEN)
        {
          if (object_visible (ob))
            {
              return ob;
            }
        }
      else
        return ob;
      ob = ob->next_inv;
    }
  return 0;
}
#endif

int get_machine_state() {
  if (!start_of_stack || !control_stack)
    return -1; /* stack machine not yet initialized */
  if (!master_ob)
    return MS_PRE_MUDLIB;
  if (current_time == 0)
    return MS_MUDLIB_LIMBO;
  return MS_MUDLIB_INTERACTIVE;
}

void setup_simulate() {
  init_otable (CONFIG_INT (__OBJECT_HASH_TABLE_SIZE__));		/*lib/lpc/otable.c */
  init_objects ();              /* lib/lpc/object.c */
  init_precomputed_tables ();   /* backend.c */
  init_binaries ();             /* lib/lpc/program/binaries.c */
  init_uids();                  /* uids.c */
  reset_interpreter ();             /* interpret.c */
  current_time = time (NULL);
}

void tear_down_simulate() {

  if (MAIN_OPTION(pedantic))
    {
      opt_trace (TT_MEMORY|2, "destructing all objects");
      current_object = master_ob;
      /* destruct everything until only master_ob and simul_efun_ob are left */
      while (obj_list)
        {
          if ((obj_list == simul_efun_ob) || (obj_list == master_ob))
            {
              obj_list = obj_list->next_all;
              continue;
            }
          remove_all_call_out (obj_list);
          destruct_object (obj_list);
        }
      obj_list = master_ob;
      if (obj_list && simul_efun_ob)
        obj_list->next_all = simul_efun_ob;
    }

  if (master_ob) {
      CLEAR_CONFIG_STR(__MASTER_FILE__); /* do not reload master_ob */
      current_object = master_ob;
      destruct_object (master_ob);
      set_master (0);
  }

  /* simul_efun_ob, if loaded, must be the LAST object to be destructed in the simulated virtual world
   * because the LPC compiler uses index of simul_efuns in the generated opcode. The program_t of
   * simul_efun_ob must remain unchanged or the generated opcode will be corrupted.
   */
  if (simul_efun_ob) {
      CLEAR_CONFIG_STR(__SIMUL_EFUN_FILE__); /* do not reload simul_efun_ob */
      current_object = simul_efun_ob;
      destruct_object (simul_efun_ob);
      set_simul_efun (0);
  }
  remove_destructed_objects(); // actually free destructed objects
  clear_apply_cache(); // clear shared strings referenced by apply cache

  reset_interpreter ();   // clear stack machine
  if (total_num_prog_blocks)
    {
      opt_trace (TT_MEMORY|1, "leaked program blocks: %zu\n", total_num_prog_blocks);
    }

  deinit_uids();      // free all uids
  deinit_objects();   // free living name hash table
  deinit_otable();    // free object name hash table
}
