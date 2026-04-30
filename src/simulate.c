#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#define SUPPRESS_COMPILER_INLINES
#include "std.h"
#include "rc.h"
#include "command.h"
#include "frame.h"
#include "interpret.h"
#include "addr_resolver.h"
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
#ifdef PACKAGE_CURL
#include "curl/curl_efuns.h"
#endif
#include "efuns/call_out.h"
#include "efuns/ed.h"
#include "efuns/file_utils.h"
#include "efuns/replace_program.h"

#include <sys/stat.h>

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif /* HAVE_STDARG_H */

/* -1 indicates that we have never had a master object.  This is so the
 * simul_efun object can load before the master. */
object_t *master_ob = 0;

object_t *obj_list, *obj_list_destruct;
object_t *current_object;	/* The object interpreting a function. */
object_t *previous_ob;  /* The object that called the current_object. */
object_t *command_giver;	/* Where the current command came from. */
object_t *current_interactive;	/* The user who caused this execution */

static int give_uid_to_object (object_t *);
static int init_object (object_t *);
static object_t *load_virtual_object (const char *);
static char *make_new_name (const char *);
static void send_say (object_t *, const char *, array_t *);

/*********************************************************************/

/**
 * @brief Check that a string is legal for printing.
 * 
 * If the string is too long, an error is raised.
 * @param s The string to check.
 */
static void check_legal_string (const char *s) {
  if (strlen (s) >= LARGEST_PRINTABLE_STRING)
    {
      error ("*Printable strings limited to length of %d.\n",
             LARGEST_PRINTABLE_STRING);
    }
}


/**
 *  @brief Give the correct uid and euid to a created object.
 * 
 *  An object must have a uid. The euid may be NULL.
 */
static int give_uid_to_object (object_t * ob) {
  svalue_t *ret;
  const char *creator_name = NULL;

  /* before master object is loaded */
  if (mud_state() < MS_MUDLIB_LIMBO)
    {
      ob->uid = add_uid ("NONAME");
      ob->euid = NULL;
      return 1;
    }

  /* ask master object who the creator of this object is */
  push_malloced_string (add_slash (ob->name));
  ret = APPLY_SLOT_MASTER_CALL (APPLY_CREATOR_FILE, 1);

  if (ret == (svalue_t *) - 1)
    {
      APPLY_SLOT_FINISH_CALL();
      destruct_object (ob);
      error ("*Can't load objects without a master object.");
      return 1;
    }

  if (ret && ret->type == T_STRING)
    creator_name = SVALUE_STRPTR(ret);

  if (!creator_name)
    creator_name = "NONAME";

  /*
   * Now we are sure that we have a creator name. It is a stack slot that lives
   * until APPLY_SLOT_FINISH_CALL() below, so it is safe to refer to it after the call.
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
          APPLY_SLOT_FINISH_CALL();
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
          APPLY_SLOT_FINISH_CALL();
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
  APPLY_SLOT_FINISH_CALL();
  return 1;
}


static int init_object (object_t * ob) {
  return give_uid_to_object (ob);
}


static object_t *load_virtual_object (const char *name) {
  svalue_t *v;
  object_t *result = 0;

  if (mud_state() < MS_MUDLIB_LIMBO)
    return 0;
  push_malloced_string (add_slash (name));
  v = APPLY_SLOT_MASTER_CALL (APPLY_COMPILE_OBJECT, 1);
  if (v && (v->type == T_OBJECT))
    result = v->u.ob;
  APPLY_SLOT_FINISH_CALL();
  return result;
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
  const char *uid = NULL;

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

  ret = APPLY_SLOT_MASTER_CALL (APPLY_GET_ROOT_UID, 0);
  if (ret && (ret->type == T_STRING))
    uid = SVALUE_STRPTR(ret);

  if (first_load)
    {
      if (uid)
        {
          master_ob->uid = set_root_uid (uid);
          master_ob->euid = master_ob->uid;
        }

      APPLY_SLOT_FINISH_CALL();

      /* The backbone UID is set only when the master object is first loaded.
       * If the master object changes later, the backbone UID remains the same
       * because there could be already objects created with that UID.
       * 
       * Retain the original backbone UID to allow new objects created by
       * backbone (as indicated by creator_file) to receive UID and EUID of
       * current_object.
       */
      ret = APPLY_SLOT_MASTER_CALL (APPLY_GET_BACKBONE_UID, 0);
      if (ret && (ret->type == T_STRING))
        set_backbone_uid (SVALUE_STRPTR(ret));
      APPLY_SLOT_FINISH_CALL();
    }
  else if (uid)
    {
      master_ob->uid = add_uid (uid);
      master_ob->euid = master_ob->uid;
      APPLY_SLOT_FINISH_CALL();
    }
  else
    {
      APPLY_SLOT_FINISH_CALL();
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
 * @param[IN] obj_name The name of the object to load. Leading slashes
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
object_t* load_object (const char *obj_name, const char *pre_text) {

  int f;
  program_t *prog = NULL;
  object_t *ob, *save_command_giver = command_giver;
  svalue_t *mret;
  struct stat c_st;
  char real_name[PATH_MAX], name[PATH_MAX - 2];

  if (++num_objects_this_thread > CONFIG_INT (__INHERIT_CHAIN_SIZE__))
    error ("*Inherit chain too deep: > %d when trying to load '%s'.", CONFIG_INT (__INHERIT_CHAIN_SIZE__), obj_name);

  if (!strip_name (obj_name, name, sizeof (name)))
    error ("*Filenames with consecutive /'s in them aren't allowed (%s).", obj_name);

  if (mud_state() >= MS_MUDLIB_LIMBO)
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
      if ((ob = load_virtual_object (name)))
        {
          /* A virtual object is returned by the master object.
          * We don't care about its actual filename, just the object.
          * Replace the object's name with the requested name and update it in the object hash table.
          */
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

  /* Get the program by loading from binary or compiling from the source.
   * Skip binary load if pre_text is provided, since the cached binary was compiled
   * without the injected code.
   */
  if (!pre_text) {
    prog = load_binary (real_name, 0);
  }

  if (!prog && !inherit_file)
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

  if (mud_state() >= MS_MUDLIB_LIMBO)
    {
      opt_trace (TT_COMPILE|3, "calling master apply: valid_object() for: \"%s\"", name);
      push_object (ob);
      mret = APPLY_SLOT_MASTER_CALL (APPLY_VALID_OBJECT, 1);
      if (mret && !MASTER_APPROVED (mret))
        {
          APPLY_SLOT_FINISH_CALL();
          destruct_object (ob);
          error ("*master::%s() denied permission to load '/%s'.", APPLY_VALID_OBJECT, name);
        }
      APPLY_SLOT_FINISH_CALL();
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
              if (!(new_ob = load_virtual_object (str1)))
                return 0;
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
 * With no argument, present() looks in the inventory of the current_object,
 * the inventory of our super, and our super.
 * If the second argument is nonzero, only the inventory of that object
 * is searched.
 */


static object_t *object_present2 (const char *, object_t *);

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

  ret_ob = object_present2 (SVALUE_STRPTR(v), ob->contains);

  if (ret_ob)
    return ret_ob;

  if (specific)
    return 0;

  if (ob->super)
    {
      push_svalue (v);
      ret = APPLY_SLOT_CALL (APPLY_ID, ob->super, 1, ORIGIN_DRIVER);

      if (ob->super->flags & O_DESTRUCTED)
        {
          APPLY_SLOT_FINISH_CALL();
          return 0;
        }

      if (!IS_ZERO (ret))
        {
          APPLY_SLOT_FINISH_CALL();
          return ob->super;
        }

      APPLY_SLOT_FINISH_CALL();

      return object_present2 (SVALUE_STRPTR(v), ob->super->contains);
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
static object_t* object_present2 (const char *str, object_t * ob) {

  svalue_t *ret;
  malloc_str_t name;
  size_t count = 0, length;

  if ((length = strlen (str)))
    {
      const unsigned char* scan = (const unsigned char*)str + length - 1;
      if (isdigit (*scan))
        {
          do
            {
              scan--;
            }
          while (scan > (const unsigned char*)str && isdigit (*scan));

          if (*scan == ' ')
            {
              count = atoi ((const char*)scan + 1) - 1;
              length = scan - (const unsigned char*)str;
            }
        }
    }

  for (; ob; ob = ob->next_inv)
    {
      name = new_string (length, "object_present2");
      memcpy (name, str, length);
      name[length] = 0;

      push_malloced_string (name);
      ret = APPLY_SLOT_CALL (APPLY_ID, ob, 1, ORIGIN_DRIVER);

      if (ob->flags & O_DESTRUCTED)
        {
          APPLY_SLOT_FINISH_CALL();
          return 0;
        }

      if (IS_ZERO (ret))
        {
          APPLY_SLOT_FINISH_CALL();
          continue;
        }

      APPLY_SLOT_FINISH_CALL();

      if (count-- > 0)
        continue;

      return ob;
    }

  return 0;
}

/**
 * @brief Load and initialize the master object with optional pre-text.
 * @param master_file Path to master file.
 * @param pre_text Optional pre-text (extra code prepended before master source).
 *
 * If pre_text is provided, it is prepended to the master source code during compilation,
 * allowing tests to inject custom code (e.g., instrumentation, mock applies) into the master.
 */
void init_master (const char *master_file, const char *pre_text) {
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

  new_ob = load_object (buf, pre_text);
  if (new_ob == 0)
    {
      fprintf (stderr, "The master file %s was not loaded.\n", master_file);
      exit (-1);
    }
  set_master (new_ob);
}

static object_t *saved_name_ob = NULL;
static char *saved_name_value = "";
static object_t *vital_destruct_guard = NULL;

static void fix_object_names (void) {
  if (saved_name_ob)
    {
      saved_name_ob->name = saved_name_value;
      saved_name_ob = NULL;
      saved_name_value = "";
      vital_destruct_guard = NULL;
    }
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

  /*
   * Destruction is a two-stage process in this driver:
   * 1) detach and sanitize runtime references immediately (this function)
   * 2) defer final object variable/prog release to remove_destructed_objects()
   *
   * This ordering keeps stack/apply code safe when destruction happens during
   * nested LPC execution.
   */
  DEBUG_CHECK (!ob, "destruct_object() called with NULL pointer.\n");

  /*
   * Guard against illegal recursive destruct requests coming from move_or_destruct.
   * The restrict_destruct token is temporary and intentionally narrow in scope.
   */
  opt_trace (TT_EVAL|1, "start destructing: /%s", ob->name);
  if (restrict_destruct && restrict_destruct != ob)
    error ("*Only this_object() can be destructed from move_or_destruct.");

  /*
   * simul_efun is special: bytecode embeds simul indexes, so while master exists
   * we reject direct simul destruction to avoid corrupting live call targets.
   */
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

#ifdef PACKAGE_CURL
  close_curl_handles (ob);
#endif

  if (ob->flags & O_DESTRUCTED)
    {
      /* Idempotent API: repeated destruction requests are treated as no-op. */
      opt_trace (TT_EVAL|1, "object /%s already destructed", ob->name);
      return;
    }

  /*
   * Drop stack-visible references first so active execution no longer points
   * at this object as a live target.
   */
  remove_object_from_stack (ob);

  /*
   * Evict inventory before unlinking from world lists.
   * This preserves move/destruct semantics for contained objects and keeps
   * environment callbacks consistent.
   */
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
          (void) APPLY_CALL (APPLY_MOVE, ob->contains, 1, ORIGIN_DRIVER);
          restrict_destruct = save_restrict_destruct;

          /* APPLY_MOVE may destruct us as a side effect; stop immediately if so. */
          if (ob->flags & O_DESTRUCTED)
            return;
        }

      /* If move callback left object in place, force destruction to make progress. */
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

  /*
   * Unlink from environment and tear down action sentences visible to nearby
   * command-enabled objects. This prevents stale command routes.
   */
  if (ob->super)
    {
      /* if we are carried by an object that enables commands, remove our sentences */
      if (ob->super->flags & O_ENABLE_COMMANDS)
        remove_sent (ob, ob->super);

      for (pp = &ob->super->contains; *pp;)
        {
          /* remove our sentences from objects in the same environment */
          if ((*pp)->flags & O_ENABLE_COMMANDS)
            remove_sent (ob, *pp);

          if (*pp != ob)
            pp = &(*pp)->next_inv;
          else
            *pp = (*pp)->next_inv;
        }
      opt_trace (TT_EVAL|1, "moved /%s out of environment", ob->name);
    }

  /*
   * Remove from object-name hash table.
   * Vital objects (master/simul) are replaced atomically: load replacement,
   * retarget global pointer, then remove old object without touching the new
   * entry that has the same canonical name.
   */
  if ((ob == master_ob) || (ob == simul_efun_ob))
    {
      object_t *new_ob = NULL;
      char *tmp = ob->name; /* a shared string */
      char *vital_obj_name = NULL;

      /* Dedicated reentrancy guard for vital-object swap path. */
      if (vital_destruct_guard)
        {
          error ("*Nested vital object destruct is not allowed.");
        }

      /*
       * Register an unwind handler before we blank the name; if an error jumps
       * out, fix_object_names() restores the original name and guard state.
       */
      (++sp)->type = T_ERROR_HANDLER;
      sp->u.error_handler = fix_object_names;
      saved_name_ob = ob;
      saved_name_value = tmp;
      vital_destruct_guard = ob;

      /* Temporarily hide old name so lookups during reload cannot select old object. */
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
                saved_name_ob = NULL;
                saved_name_value = "";
                vital_destruct_guard = NULL;
              sp--;
              error ("*Destruction of vital object rejected due to invalid config setting (\"%s\").", vital_obj_name);
            }
          opt_trace (TT_EVAL|1, "reloading vital object: /%s", new_name);
          new_ob = load_object (new_name, 0);
          if (!new_ob)
            {
              ob->name = tmp;
                saved_name_ob = NULL;
                saved_name_value = "";
                vital_destruct_guard = NULL;
              sp--;
              error ("*Destruct on vital object failed: new copy failed to reload.");
            }
        }

      if (ob == master_ob)
        {
          set_master (new_ob); /* could be NULL when unit-test or shutdown */
        }
      else if (ob == simul_efun_ob)
        {
          set_simul_efun (new_ob); /* could be NULL when unit-test or shutdown */
        }

      sp--;			/* pop T_ERROR_HANDLER */

      /*
       * Restore old name for hash removal and temporarily blank replacement name,
       * so remove_object_hash() cannot accidentally unlink the replacement object.
       */
      ob->name = tmp;
      saved_name_ob = NULL;
      saved_name_value = "";
      vital_destruct_guard = NULL;
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
   * Remove from global object list after hash/env/vital transitions complete.
   * Doing this last preserves recoverability if errors occur in earlier phases.
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

  /*
   * Free action sentences owned by this object.
   * This releases cross-object references before deferred deallocation runs,
   * avoiding reference-count leaks in command wiring.
   */
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

  /*
   * Clean input_to/get_char callback references pointing at this object.
   * Otherwise dormant interactives can retain a dead object indefinitely.
   */
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

  /*
   * Final detach: object is no longer command-capable or reachable from world
   * topology, and enters deferred-destruction list.
   */
  ob->flags &= ~O_ENABLE_COMMANDS;
  ob->super = 0;
  ob->next_inv = 0;
  ob->contains = 0;
  ob->next_all = obj_list_destruct;
  obj_list_destruct = ob;

  set_heart_beat (ob, 0);
  /* Mark after detaching to keep traversal code from treating it as live. */
  ob->flags |= O_DESTRUCTED; /* mark as destructed */

  /* moved this here from destruct2() -- see comments in destruct2() */
  if (ob->interactive)
    {
      opt_trace (TT_COMM|1, "disconnecting /%s as interactive object", ob->name);
      remove_interactive (ob, true);
    }
  opt_trace (TT_EVAL|1, "finished destructing: /%s", ob->name);
}


/*
 * Deferred destruction stage.
 *
 * Preconditions:
 * - object was detached and marked O_DESTRUCTED in destruct_object()
 * - no LPC code is currently executing on this object
 *
 * Responsibility:
 * - release object-owned runtime values safely
 * - then drop the final object reference via free_object()
 */
static void destruct2 (object_t * ob) {
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
    /* Not fatal: external references can legally postpone final memory release. */
    opt_warn (1, "object /%s has ref count %d\n", ob->name, ob->ref);

  /*
   * This decrements the reference count of the object. If the object is referenced
   * by other objects, it will not be freed until the last reference is gone. If the
   * object is not referenced by any other objects, it will be freed immediately.
   */
  free_object (ob, "destruct2");
}

/*
 * Flush deferred destruction queue.
 *
 * Objects are first detached in destruct_object(), then consumed here after the
 * active execution window ends. This avoids freeing structures that may still be
 * observed transiently by in-flight driver code.
 */

void remove_destructed_objects () {
  object_t *ob, *next;

  /* Apply pending replace_program requests before reclaiming object programs. */
  if (obj_list_replace)
    replace_programs ();

  /* Walk stable next pointers; destruct2() may free current object immediately. */
  for (ob = obj_list_destruct; ob; ob = next)
    {
      next = ob->next_all;
      destruct2 (ob);
    }

  /* Queue fully drained for next driver tick. */
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

static void send_say (object_t * ob, const char *text, array_t * avoid) {
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
  const char *buff;

  check_legal_string (SVALUE_STRPTR(v));
  buff = SVALUE_STRPTR(v);

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

static void tell_npc (object_t * ob, const char *str) {
  copy_and_push_string (str);
  APPLY_CALL (APPLY_CATCH_TELL, ob, 1, ORIGIN_DRIVER);
}

/**
 * @brief Send a message to an object, either to its screen if it's interactive or
 * to its catch_tell() function if it's not.
 * 
 * tell_object: send a message to an object.
 * If it is an interactive object, it will go to his
 * screen. Otherwise, it will go to a local function
 * catch_tell() in that object. This enables communications
 * between users and NPC's, and between other NPC's.
 * If INTERACTIVE_CATCH_TELL is defined then the message always
 * goes to catch_tell unless the target of tell_object is interactive
 * and is the current_object in which case it is written via add_message().
 */
void tell_object (object_t * ob, const char *str) {
  if (!ob || (ob->flags & O_DESTRUCTED))
    {
      add_message (0, str);
      debug_message ("*%s", str);
      return;
    }

  /* [NEOLITH-EXTENSION] master_ob can be used as a user object when in single-user mode */
  if ((ob == simul_efun_ob) || ((ob == master_ob) && !master_ob->interactive))
    {
      debug_message ("*%s", str);
      return;
    }

  /* if this is on, EVERYTHING goes through catch_tell() */
#ifndef INTERACTIVE_CATCH_TELL
  if (ob->interactive)
    add_message (ob, str);
  else
#endif
    tell_npc (ob, str);
}

/*
 * Sends a string to all objects inside of a specific object.
 * Revised, bobf@metronet.com 9/6/93
 */
#ifdef F_TELL_ROOM
void tell_room (object_t * room, svalue_t * v, array_t * avoid) {
  object_t *ob, *next_ob;
  const char *buff;
  int valid, j;
  char txt_buf[LARGEST_PRINTABLE_STRING];

  switch (v->type)
    {
    case T_STRING:
      check_legal_string (SVALUE_STRPTR(v));
      buff = SVALUE_STRPTR(v);
      break;
    case T_OBJECT:
      buff = v->u.ob->name;
      break;
    case T_NUMBER:
      sprintf (txt_buf, "%" PRId64, v->u.number);
      buff = txt_buf;
      break;
    case T_REAL:
      sprintf (txt_buf, "%g", v->u.real);
      buff = txt_buf;
      break;
    default:
      bad_argument (v, T_OBJECT | T_NUMBER | T_REAL | T_STRING,
                    2, F_TELL_ROOM);
      IF_DEBUG (buff = 0);
      return;
    }

  for (ob = room->contains; ob; ob = next_ob)
    {
      if (ob->flags & O_DESTRUCTED)
        {
          /* TODO: resume next object for tell_room */
          break;
        }
      next_ob = ob->next_inv; /* in case ob is destructed during tell_object() */
      if (!ob->interactive && !(ob->flags & O_LISTENER))
        continue;

      /* skip objects in the avoid array */
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

      /* tell the object */
      tell_object (ob, buff);
    }
}
#endif

void shout_string (const char *str) {
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
 *    accessible by interactive users. The \p command_giver will be set to the
 *    object when commands are enabled, and set back to 0 when disabled.
 *  @param enable If non-zero, enable commands, else disable commands.
 */
void enable_commands (int enable) {
  if (current_object->flags & O_DESTRUCTED)
    return;

  if (enable)
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
      opt_trace (TT_COMM|2, "set callback function to '%s' in object /%s", SVALUE_STRPTR(fun), current_object->name);
      callback_funp = make_lfun_funp_by_name (SVALUE_STRPTR(fun), &dummy); /* ref = 1, by sentence->function.f */
      if (!callback_funp)
        {
          error ("Function '%s' not found in input_to", SVALUE_STRPTR(fun));
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
      opt_trace (TT_COMM|2, "set callback function to '%s' in object /%s", SVALUE_STRPTR(fun), current_object->name);
      callback_funp = make_lfun_funp_by_name (SVALUE_STRPTR(fun), &dummy);
      if (!callback_funp)
        {
          error ("Function '%s' not found in get_char", SVALUE_STRPTR(fun));
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

static void print_svalue (svalue_t * arg) {
  char tbuf[2048];

  if (arg == 0)
    {
      tell_object (command_giver, "<NULL>");
    }
  else
    switch (arg->type)
      {
      case T_STRING:
        check_legal_string (SVALUE_STRPTR(arg));
        tell_object (command_giver, SVALUE_STRPTR(arg));
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

#ifdef F_RECEIVE
void
f_receive (void)
{
  if (current_object->interactive)
    {
      check_legal_string (SVALUE_STRPTR(sp));
      add_message (current_object, SVALUE_STRPTR(sp));
    }
  free_string_svalue (sp--);
}
#endif


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
      (void) APPLY_CALL (APPLY_INIT, dest, 0, ORIGIN_DRIVER);
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
          (void) APPLY_CALL (APPLY_INIT, item, 0, ORIGIN_DRIVER);
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
          (void) APPLY_CALL (APPLY_INIT, ob, 0, ORIGIN_DRIVER);
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
      (void) APPLY_CALL (APPLY_INIT, item, 0, ORIGIN_DRIVER);
    }

  command_giver = save_cmd;
}

/* fatal() - Fatal error handler
 *
 * Prints error message to debug log and exit program.
 * */
static int proceeding_fatal_error = 0;

void fatal (const char *fmt, ...) {
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

      proceeding_fatal_error = 1;

      if (current_file)
        debug_message ("{}\t----- compiling %s at line %d", current_file, current_line);

      if (current_object)
        debug_message ("{}\t----- current object was /%s", current_object->name);

      if ((ob_name = dump_trace (DUMP_WITH_ARGS | DUMP_WITH_LOCALVARS)))
        debug_message ("{}\t----- in heart beat of /%s", ob_name);

      stem_crash_handler(msg);
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
 * backend loop will call this function to perform the actual shutdown.
 *
 * @return After this function returns, the driver process should exit.
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

  /*
   * NOTE: We do not do active tear down of the runtime environment when running as
   * a long-lived server process. It is not pratical to require the mudlib to destruct
   * all loaded objects and free all allocated resources in a clean way upon shutdown.
   * All allocated resources will be reclaimed by the operating system upon process
   * termination.
   *
   * In some cases, the mudlib author would like to do clean up for testing or
   * debugging purposes. The --pedantic command line option (-p) is provided for this
   * purpose.
   *
   * However, we do call tear down after running unit tests to ensure that there
   * is no memory leak. The graceful tear down code can be found in various unit-testing
   * code under the tests/ directory.
   */
  if (MAIN_OPTION(pedantic))
    {
      debug_message ("{}\ttearing down world simulation");
      tear_down_simulate();
      debug_message ("{}\tdeinitializing all subsystems");
      deinit_lpc_compiler();
      deinit_strings();
      deinit_config();
    }

#ifdef PROFILING
  monitor (0, 0, 0, 0, 0);	/* cause gmon.out to be written */
#endif
}

/**
 * Call this one when there is only little memory left. It will start
 * Armageddon.
 */
void initiate_slow_shutdown (int minutes) {
  /*
   * Swap out objects, and free some memory.
   */
  svalue_t *amo;

  push_number (minutes);
  amo = APPLY_SLOT_MASTER_CALL (APPLY_SLOW_SHUTDOWN, 1);
  /* in this case, approved means the mudlib will handle it */
  if (!MASTER_APPROVED (amo))
    {
      APPLY_SLOT_FINISH_CALL();
      object_t *save_current = current_object, *save_command = command_giver;

      command_giver = 0;
      current_object = 0;
      shout_string ("Out of memory.\n");
      command_giver = save_command;
      current_object = save_current;
      g_proceeding_shutdown = true;
      return;
    }
  APPLY_SLOT_FINISH_CALL();
}


void do_message (svalue_t * msg_class, svalue_t * msg, array_t * scope, array_t * exclude, int recurse) {
  int i, j, valid;
  object_t *ob;

  for (i = 0; i < scope->size; i++)
    {
      switch (scope->item[i].type)
        {
        case T_STRING:
          ob = find_or_load_object (SVALUE_STRPTR(&scope->item[i]));
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
              APPLY_CALL (APPLY_RECEIVE_MESSAGE, ob, 2, ORIGIN_DRIVER);
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


object_t* first_inventory (svalue_t * arg) {
  object_t *ob;

  if (arg->type == T_STRING)
    {
      ob = find_or_load_object (SVALUE_STRPTR(arg));
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

/* Returns an array of all objects contained in 'ob' */
array_t* all_inventory (object_t * ob, int override_master) {
  array_t *d;
  object_t *cur;
  int cnt, res;
  int display_hidden;

  if (override_master)
    {
      display_hidden = 1;
    }
  else
    {
      display_hidden = -1;
    }
  cnt = 0;
  for (cur = ob->contains; cur; cur = cur->next_inv)
    {
      if (cur->flags & O_HIDDEN)
        {
          if (display_hidden == -1)
            {
              display_hidden = valid_hide (current_object);
            }
          if (display_hidden)
            cnt++;
        }
      else
        cnt++;
    }

  if (!cnt)
    return &the_null_array;

  d = allocate_empty_array (cnt);
  cur = ob->contains;

  for (res = 0; res < cnt; res++)
    {
      if ((cur->flags & O_HIDDEN) && !display_hidden)
        {
          cur = cur->next_inv;
          res--;
          continue;
        }
      d->item[res].type = T_OBJECT;
      d->item[res].u.ob = cur;
      add_ref (cur, "all_inventory");
      cur = cur->next_inv;
    }
  return d;
}

int mud_state() {
  if (proceeding_fatal_error)
    return MS_FATAL_ERROR; /* fatal error occurred, driver is shutting down */

  if (!start_of_stack || !control_stack)
    return MS_PRE_INIT; /* stack machine not yet initialized */

  if (!master_ob)
    return MS_PRE_MUDLIB; /* mudlib not yet initialized */

  if (current_time == boot_time)
    return MS_MUDLIB_LIMBO; /* `current_time` not yet advanced */

  return MS_MUDLIB_INTERACTIVE;
}

void setup_simulate() {
  if (master_ob || simul_efun_ob || obj_list || obj_list_destruct)
    fatal ("setup_simulate() called but master_ob, simul_efun_ob, obj_list or obj_list_destruct is not empty");
  init_otable (CONFIG_INT (__OBJECT_HASH_TABLE_SIZE__));		/*lib/lpc/otable.c */
  init_objects ();              /* lib/lpc/object.c */
  init_precomputed_tables ();   /* backend.c */
  init_binaries ();             /* lib/lpc/program/binaries.c */
  init_uids();                  /* uids.c */
  init_backend ();              /* backend.c */
}

void tear_down_simulate() {

  deinit_dns_system();
  addr_resolver_deinit();

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

  if (master_ob)
    {
      CLEAR_CONFIG_STR(__MASTER_FILE__); /* do not reload master_ob */
      current_object = master_ob;
      destruct_object (master_ob);
      set_master (0);
    }

  /* simul_efun_ob, if loaded, must be the LAST object to be destructed in the simulated virtual world
   * because the LPC compiler uses index of simul_efuns in the generated opcode. The program_t of
   * simul_efun_ob must remain unchanged or the generated opcode will be corrupted.
   */
  if (simul_efun_ob)
    {
      CLEAR_CONFIG_STR(__SIMUL_EFUN_FILE__); /* do not reload simul_efun_ob */
      current_object = simul_efun_ob;
      destruct_object (simul_efun_ob);
      set_simul_efun (0);
    }
  current_object = previous_ob = command_giver = 0;

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
