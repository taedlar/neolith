/*  $Id: simulate.c,v 1.2 2002/11/25 11:11:05 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#define SUPPRESS_COMPILER_INLINES
#include "std.h"
#include "lpc/array.h"
#include "lpc/mapping.h"
#include "lpc/object.h"
#include "lpc/program.h"
#include "lpc/function.h"
#include "lpc/disassemble.h"
#include "backend.h"
#include "interpret.h"
#include "simul_efun.h"
#include "otable.h"
#include "comm.h"
#include "binaries.h"
#include "socket/socket_efuns.h"
#include "efuns/operator.h"
#include "efuns/ed.h"
#include "efuns/sprintf.h"
#include "file.h"
#include "applies.h"
#include "simulate.h"
#include "stralloc.h"
#include "main.h"
#include "LPC/origin.h"
#include "LPC/runtime_config.h"
#include "rc.h"

#include <sys/stat.h>

/*
 * 'inherit_file' is used as a flag. If it is set to a string
 * after yyparse(), this string should be loaded as an object,
 * and the original object must be loaded again.
 */
char *inherit_file;

/* prevents infinite inherit loops.
   No, mark-and-sweep solution won't work.  Exercise for reader.  */
static int num_objects_this_thread = 0;

static object_t *restrict_destruct;

char *last_verb = 0;

int g_trace_flag = 0;

int illegal_sentence_action;

object_t *obj_list, *obj_list_destruct;
object_t *current_object;	/* The object interpreting a function. */
object_t *command_giver;	/* Where the current command came from. */
object_t *current_interactive;	/* The user who caused this execution */

static int give_uid_to_object (object_t *);
static int init_object (object_t *);
static svalue_t *load_virtual_object (char *);
static char *make_new_name (char *);
static void send_say (object_t *, char *, array_t *);
static sentence_t *alloc_sentence (void);
static void remove_sent (object_t *, object_t *);


/*************************************************************************
 *  command_giver_stack
 *
 *  這個堆疊用來暫存 command_giver 指標。主要用途是在呼叫 LPC 程式之前，
 *  將目前的 command_giver 推入堆疊 (並可設定新的 command_giver)，在 LPC
 *  程式返回之後，由堆疊取出 command_giver。這樣即使 LPC 程式將 command_giver
 *  摧毀了，我們也能取得 command_giver 的指標。
 */

static object_t *command_giver_stack[1024];	/* 堆疊 */
static object_t **cgsp = command_giver_stack;	/* 指向堆疊頂端 */

void
save_command_giver (object_t * new_command_giver)
{
  /* 檢查堆疊是不是滿了 */
  if (cgsp >= EndOf (command_giver_stack))
    fatal (_("*****Command giver stack overflow!"));

  /* 將原 command_giver 推入堆疊 */
  *(++cgsp) = command_giver;

  /* 設定新的 command_giver (可以為 NULL), 並將參考計數加 1 */
  if (new_command_giver)
    add_ref (new_command_giver, "save_command_giver");
  command_giver = new_command_giver;
}

void
restore_command_giver ()
{
  /* 將目前的 command_giver 參考計數減 1 */
  if (command_giver)
    free_object (command_giver, "restore_command_giver");

  /* 檢查堆疊是不是空的 */
  if (cgsp == command_giver_stack)
    fatal (_("*****Command giver stack underflow!"));

  /* 從堆疊取回 command_giver */
  command_giver = *(cgsp--);
}

/*********************************************************************/

inline void
check_legal_string (char *s)
{
  if (strlen (s) >= LARGEST_PRINTABLE_STRING)
    {
      error (_("*Printable strings limited to length of %d.\n"),
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
char *
strput (char *x, char *limit, char *y)
{
  while ((*x++ = *y++))
    {
      if (x == limit)
	{
	  *(x - 1) = 0;
	  break;
	}
    }
  return x - 1;
}

char *
strput_int (char *x, char *limit, int num)
{
  char buf[20];
  sprintf (buf, "%d", num);
  return strput (x, limit, buf);
}


/*
 * Give the correct uid and euid to a created object.
 */
static int
give_uid_to_object (object_t * ob)
{
  svalue_t *ret;
  char *creator_name;

  /* before master object is loaded */
  if (!master_ob)
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
      error (_("*Can't load objects without a master object."));
      return 1;
    }

  if (ret && ret->type == T_STRING)
    creator_name = ret->u.string;
  else
    creator_name = "NONAME";

  /*
   * Now we are sure that we have a creator name. Do not call apply()
   * again, because creator_name will be lost !
   */
  if (strcmp (current_object->uid->name, creator_name) == 0)
    {
      /*
       * The loaded object has the same uid as the loader.
       */
      ob->uid = current_object->uid;
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
      return 1;
    }
#endif

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

  return 1;
}


static int
init_object (object_t * ob)
{
  return give_uid_to_object (ob);
}


static svalue_t *
load_virtual_object (char *name)
{
  svalue_t *v;

  if (!master_ob)
    return 0;
  push_malloced_string (add_slash (name));
  v = apply_master_ob (APPLY_COMPILE_OBJECT, 1);
  if (!v || (v->type != T_OBJECT))
    return 0;
  return v;
}


void
set_master (object_t * ob)
{
  int first_load = (!master_ob);
  svalue_t *ret;
  char *root_uid = NULL;

  master_ob = ob;

  /* Make sure master_ob is never made a dangling pointer. */
  add_ref (master_ob, "set_master");

  ret = apply_master_ob (APPLY_GET_ROOT_UID, 0);
  if (ret && (ret->type == T_STRING))
    root_uid = ret->u.string;

  if (first_load)
    {
      if (root_uid)
	{
	  master_ob->uid = set_root_uid (root_uid);
	  master_ob->euid = master_ob->uid;
	}

      ret = apply_master_ob (APPLY_GET_BACKBONE_UID, 0);
      if (ret && (ret->type == T_STRING))
	set_backbone_uid (ret->u.string);
    }
  else if (root_uid)
    {
      master_ob->uid = add_uid (root_uid);
      master_ob->euid = master_ob->uid;
    }
}


char *
check_name (char *src)
{
  char *p;
  while (*src == '/')
    src++;
  p = src;
  while (*p)
    {
      if (*p == '/' && *(p + 1) == '/')
	return 0;
      p++;
    }
  return src;
}

int
strip_name (char *src, char *dest, int size)
{
  char last_c = 0;
  char *p = dest;
  char *end = dest + size - 1;

  while (*src == '/')
    src++;

  while (*src && p < end)
    {
      if (last_c == '/' && *src == '/')
	return 0;
      last_c = (*p++ = *src++);
    }

  /* In some cases, (for example, object loading) this currently gets
   * run twice, once in find_object, and once in load object.  The
   * net effect of this is:
   * /foo.c -> /foo [no such exists, try to load] -> /foo created
   * /foo.c.c -> /foo.c [no such exists, try to load] -> /foo created
   *
   * causing a duplicate object crash.  There are two ways to fix this:
   * (1) strip multiple .c's so that the output of this routine is something
   *     that doesn't change if this is run again.
   * (2) make sure this routine is only called once on any name.
   *
   * The first solution is the one currently in use.
   */
  while ((p - dest > 2) && (p[-1] == 'c') && (p[-2] == '.'))
    p -= 2;

  *p = 0;
  return 1;
}


/*
 * Load an object definition from file. If the object wants to inherit
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
 * Save the command_giver, because reset() in the new object might change
 * it.
 *
 */
object_t *
load_object (char *lname)
{
  int f;
  program_t *prog;
  object_t *ob, *save_command_giver = command_giver;
  svalue_t *mret;
  struct stat c_st;
  char real_name[200], name[200];

  if (++num_objects_this_thread > CONFIG_INT (__INHERIT_CHAIN_SIZE__))
    error (_("*Inherit chain too deep: > %d when trying to load '%s'."),
	   CONFIG_INT (__INHERIT_CHAIN_SIZE__), lname);

  if (current_object && current_object!=master_ob &&
      current_object->euid == NULL)
    error (_("*Can't load objects when no effective user."));

  if (!strip_name (lname, name, sizeof name))
    error (_("*Filenames with consecutive /'s in them aren't allowed (%s)."),
	   lname);

  /*
   * First check that the c-file exists.
   */
  (void) strcpy (real_name, name);
  (void) strcat (real_name, ".c");

  opt_trace (TT_COMPILE, "file: /%s", real_name);

  if (stat (real_name, &c_st) == -1)
    {
      svalue_t *v;

      if (!(v = load_virtual_object (name)))
	{
	  num_objects_this_thread--;
	  return 0;
	}
      /* Now set the file name of the specified object correctly... */
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

  /*
   * Check if it's a legal name.
   */
  if (!legal_path (real_name))
    {
      debug_message ("Illegal pathname: /%s\n", real_name);
      error (_("*Illegal path name '/%s'."), real_name);
      return 0;
    }

  if (!(prog = load_binary (real_name, lpc_obj)) && !inherit_file)
    {
      /* maybe move this section into compile_file? */
      if (comp_flag)
	{
	  debug_message (" compiling /%s ...", real_name);
	}
      f = open (real_name, O_RDONLY);
      if (f == -1)
	{
	  debug_perror ("compile_file", real_name);
	  error (_("*Could not read the file '/%s'."), real_name);
	}
      prog = compile_file (f, real_name);

      if (comp_flag)
	debug_message (" done\n");
      update_compile_av (total_lines);
      total_lines = 0;
      close (f);
    }

  /* Sorry, can't handle objects without programs yet. */
  if (inherit_file == 0 && (num_parse_error > 0 || prog == 0))
    {
      if (prog)
	free_prog (prog, 1);
      if (num_parse_error == 0 && prog == 0)
	error (_("*No program in object '/%s'!"), name);
      error (_("*Error in loading object '/%s':"), name);
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
	  error (_("*Illegal to inherit self."));
	}

      if ((inh_obj = lookup_object_hash (inhbuf)))
	{
	  IF_DEBUG (fatal ("*****Inherited object is already loaded!"));
	}
      else
	{
	  inh_obj = load_object (inhbuf);
	}
      if (!inh_obj)
	error (_("*Inherited file '/%s' does not exist!"), inhbuf);

      /*
       * Yes, the following is necessary.  It is possible that when we
       * loaded the inherited object, it loaded this object from it's
       * create function. Without this check, that would crash the driver.
       * -Beek
       */
      if (!(ob = lookup_object_hash (name)))
	{
	  ob = load_object (name);
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
  ob = get_empty_object (prog->num_variables_total);
  /* Shared string is no good here */
  ob->name = alloc_cstring (name, "load_object");
  ob->prog = prog;
  ob->flags |= O_WILL_RESET;	/* must be before reset is first called */
  ob->next_all = obj_list;
  obj_list = ob;
  enter_object_hash (ob);	/* add name to fast object lookup table */
  push_object (ob);
  mret = apply_master_ob (APPLY_VALID_OBJECT, 1);
  if (mret && !MASTER_APPROVED (mret))
    {
      destruct_object (ob);
      error (_("*master::%s() denied permission to load '/%s'."), APPLY_VALID_OBJECT, name);
    }

  if (init_object (ob))
    call_create (ob, 0);
  if (!(ob->flags & O_DESTRUCTED) && function_exists (APPLY_CLEAN_UP, ob, 1))
    {
      ob->flags |= O_WILL_CLEAN_UP;
    }
  command_giver = save_command_giver;
  ob->load_time = current_time;
  num_objects_this_thread--;
  return ob;
}


static char *
make_new_name (char *str)
{
  static int i;
  char *p = DXALLOC (strlen (str) + 10, TAG_OBJ_NAME, "make_new_name");

  (void) sprintf (p, "%s#%d", str, i);
  i++;
  return p;
}


/*
 * Save the command_giver, because reset() in the new object might change
 * it.
 */
object_t *
clone_object (char *str1, int num_arg)
{
  object_t *ob, *new_ob;
  object_t *save_command_giver = command_giver;

  if (current_object && current_object->euid == 0)
    {
      /* 允許 master object 在沒有 effective UID 狀況下載入物件 */
      if (current_object != master_ob)
	error (_("*Attempt to create object without effective UID."));
    }
  num_objects_this_thread = 0;
  ob = find_object (str1);
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
	error (_("*Cannot clone from a clone!"));
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

	  if (!(str1 = check_name (str1)))
	    error (_("*Filenames with consecutive /'s in them aren't allowed (%s)."),
	       str1);

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
  new_ob->flags |= (O_CLONE | (ob->flags & (O_WILL_CLEAN_UP | O_WILL_RESET)));
  new_ob->load_time = ob->load_time;
  new_ob->prog = ob->prog;
  reference_prog (ob->prog, "clone_object");
  DEBUG_CHECK (!current_object, "clone_object() from no current_object !\n");

  init_object (new_ob);

  new_ob->next_all = obj_list;
  obj_list = new_ob;
  enter_object_hash (new_ob);	/* Add name to fast object lookup table */
  call_create (new_ob, num_arg);
  command_giver = save_command_giver;
  /* Never know what can happen ! :-( */
  if (new_ob->flags & O_DESTRUCTED)
    return (0);
  return (new_ob);
}

object_t *
environment (svalue_t * arg)
{
  object_t *ob = current_object;

  if (arg && arg->type == T_OBJECT)
    ob = arg->u.ob;
  if (ob == 0 || ob->super == 0 || (ob->flags & O_DESTRUCTED))
    return 0;
  if (ob->flags & O_DESTRUCTED)
    error (_("*environment() of destructed object."));
  return ob->super;
}

/*
 * Execute a command for an object. Copy the command into a
 * new buffer, because 'parse_command()' can modify the command.
 * If the object is not current object, static functions will not
 * be executed. This will prevent forcing users to do illegal things.
 *
 * Return cost of the command executed if success (> 0).
 * When failure, return 0.
 */
int
command_for_object (char *str)
{
  char buff[1000];
  int save_eval_cost = eval_cost;

  if (strlen (str) > sizeof (buff) - 1)
    error (_("*Too long command."));
  else if (current_object->flags & O_DESTRUCTED)
    return 0;
  strncpy (buff, str, sizeof buff);
  buff[sizeof buff - 1] = '\0';
  if (parse_command (buff, current_object))
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

object_t *
object_present (svalue_t * v, object_t * ob)
{
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

static object_t *
object_present2 (char *str, object_t * ob)
{
  svalue_t *ret;
  char *p;
  int count = 0, length;

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

void
init_master (char *file)
{
  char buf[512];
  object_t *new_ob;

  if (!file || !file[0])
    {
      fprintf (stderr, "No master object specified in config file\n");
      exit (-1);
    }

  if (!strip_name (file, buf, sizeof buf))
    error (_("*Illegal master file name '%s'"), file);

  if (file[strlen (file) - 2] != '.')
    strcat (buf, ".c");

  new_ob = load_object (file);
  if (new_ob == 0)
    {
      fprintf (stderr, "The master file %s was not loaded.\n", file);
      exit (-1);
    }
  set_master (new_ob);
}

static char *saved_master_name;
static char *saved_simul_name;

static void
fix_object_names ()
{
  master_ob->name = saved_master_name;
  simul_efun_ob->name = saved_simul_name;
}

/*
 * Remove an object. It is first moved into the destruct list, and
 * not really destructed until later. (see destruct2()).
 */
void
destruct_object (object_t * ob)
{
  object_t **pp;
  int removed;
  object_t *super;
  object_t *save_restrict_destruct = restrict_destruct;

  if (restrict_destruct && restrict_destruct != ob)
    error (_("*Only this_object() can be destructed from move_or_destruct."));

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
    return;

  remove_object_from_stack (ob);

  /* try to move our contents somewhere */
  super = ob->super;

  while (ob->contains)
    {
      object_t *otmp;

      otmp = ob->contains;
      /*
       * An error here will not leave destruct() in an inconsistent
       * stage.
       */
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

      if (otmp == ob->contains)
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

  /* At this point, we can still back out, but this is the very last
   * minute we can do so.  Make sure we have a new object to replace
   * us if this is a vital object.
   */
  if (ob == master_ob || ob == simul_efun_ob)
    {
      object_t *new_ob;
      char *tmp = ob->name;

      (++sp)->type = T_ERROR_HANDLER;
      sp->u.error_handler = fix_object_names;
      if (master_ob)		/* this could be called before init_master() returns */
	saved_master_name = master_ob->name;
      saved_simul_name = simul_efun_ob->name;

      /* hack to make sure we don't find ourselves at several points
         in the following process */
      ob->name = "";

      /* handle these two carefully, since they are rather vital */
      new_ob = load_object (tmp);
      if (!new_ob)
	{
	  ob->name = tmp;
	  sp--;
	  error (_("*Destruct on vital object failed: new copy failed to reload."));
	}

      free_object (ob, "vital object reference");
      if (ob == master_ob)
	set_master (new_ob);
      if (ob == simul_efun_ob)
	set_simul_efun (new_ob);

      /* Set the name back so we can remove it from the hash table.
         Also be careful not to remove the new object, which has
         the same name. */
      sp--;			/* error handler */
      ob->name = tmp;
      tmp = new_ob->name;
      new_ob->name = "";
      remove_object_hash (ob);
      new_ob->name = tmp;
    }
  else
    remove_object_hash (ob);

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
  DEBUG_CHECK (!removed, "Failed to delete object.\n");

  if (ob->living_name)
    remove_living_name (ob);

  ob->flags &= ~O_ENABLE_COMMANDS;
  ob->super = 0;
  ob->next_inv = 0;
  ob->contains = 0;
  ob->next_all = obj_list_destruct;
  obj_list_destruct = ob;

  set_heart_beat (ob, 0);
  ob->flags |= O_DESTRUCTED;

  /* moved this here from destruct2() -- see comments in destruct2() */
  if (ob->interactive)
    remove_interactive (ob, 1);
}


/*
 * This one is called when no program is executing from the main loop.
 */
void
destruct2 (object_t * ob)
{
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
  free_object (ob, "destruct_object");
}

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

static void
send_say (object_t * ob, char *text, array_t * avoid)
{
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

void
say (svalue_t * v, array_t * avoid)
{
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
void
tell_room (object_t * room, svalue_t * v, array_t * avoid)
{
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
      sprintf (buff, "%d", v->u.number);
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

void
shout_string (char *str)
{
  object_t *ob;

  check_legal_string (str);

  for (ob = obj_list; ob; ob = ob->next_all)
    {
      if (!(ob->flags & O_LISTENER) || (ob == command_giver) || !ob->super)
	continue;
      tell_object (ob, str);
    }
}

/*
 * This will enable an object to use commands normally only
 * accessible by interactive users.
 * Also check if the user is a wizard. Wizards must not affect the
 * value of the wizlist ranking.
 */

void
enable_commands (int num)
{
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

/*
 * Set up a function in this object to be called with the next
 * user input string.
 */
int
input_to (svalue_t * fun, int flag, int num_arg, svalue_t * args)
{
  sentence_t *s;
  svalue_t *x;
  int i;

  if (!command_giver || command_giver->flags & O_DESTRUCTED)
    return 0;

  s = alloc_sentence ();
  if (set_call (command_giver, s, flag & ~I_SINGLE_CHAR))
    {
      /*
       * If we have args, we copy them, and adjust the stack automatically
       * (elsewhere) to avoid double free_svalue()'s
       */
      if (num_arg)
	{
	  i = num_arg * sizeof (svalue_t);
	  if ((x = (svalue_t *)
	       DMALLOC (i, TAG_INPUT_TO, "input_to: 1")) == NULL)
	    fatal ("Out of memory!\n");
	  memcpy (x, args, i);
	}
      else
	x = NULL;

      command_giver->interactive->carryover = x;
      command_giver->interactive->num_carry = num_arg;
      if (fun->type == T_STRING)
	{
	  s->function.s = make_shared_string (fun->u.string);
	  s->flags = 0;
	}
      else
	{
	  s->function.f = fun->u.fp;
	  fun->u.fp->hdr.ref++;
	  s->flags = V_FUNCTION;
	}
      s->ob = current_object;
      add_ref (current_object, "input_to");
      return 1;
    }

  free_sentence (s);
  return 0;
}


/*
 * Set up a function in this object to be called with the next
 * user input character.
 */
int
get_char (svalue_t * fun, int flag, int num_arg, svalue_t * args)
{
  sentence_t *s;
  svalue_t *x;
  int i;

  if (!command_giver || command_giver->flags & O_DESTRUCTED)
    return 0;

  s = alloc_sentence ();
  if (set_call (command_giver, s, flag | I_SINGLE_CHAR))
    {
      /*
       * If we have args, we copy them, and adjust the stack automatically
       * (elsewhere) to avoid double free_svalue()'s
       */
      if (num_arg)
	{
	  i = num_arg * sizeof (svalue_t);
	  if ((x = (svalue_t *)
	       DMALLOC (i, TAG_TEMPORARY, "get_char: 1")) == NULL)
	    fatal ("Out of memory!\n");
	  memcpy (x, args, i);
	}
      else
	x = NULL;

      command_giver->interactive->carryover = x;
      command_giver->interactive->num_carry = num_arg;
      if (fun->type == T_STRING)
	{
	  s->function.s = make_shared_string (fun->u.string);
	  s->flags = 0;
	}
      else
	{
	  s->function.f = fun->u.fp;
	  fun->u.fp->hdr.ref++;
	  s->flags = V_FUNCTION;
	}
      s->ob = current_object;
      add_ref (current_object, "get_char");
      return 1;
    }
  free_sentence (s);
  return 0;
}

void
print_svalue (svalue_t * arg)
{
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
	sprintf (tbuf, "%d", arg->u.number);
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


void
do_write (svalue_t * arg)
{
  object_t *save_command_giver = command_giver;

  if (!command_giver)
    command_giver = current_object;
  print_svalue (arg);
  command_giver = save_command_giver;
}

/* Find an object. If not loaded, load it !
 * The object may self-destruct, which is the only case when 0 will be
 * returned.
 */

object_t *
find_object (char *str)
{
  object_t *ob;
  char tmpbuf[MAX_OBJECT_NAME_SIZE];

  if (!strip_name (str, tmpbuf, sizeof tmpbuf))
    return 0;

  if ((ob = lookup_object_hash (tmpbuf)))
    return ob;

  ob = load_object (tmpbuf);
  if (!ob || (ob->flags & O_DESTRUCTED))	/* *sigh* */
    return 0;

  return ob;
}

/* Look for a loaded object. Return 0 if non found. */
object_t *
find_object2 (char *str)
{
  register object_t *ob;
  char p[MAX_OBJECT_NAME_SIZE];

  if (!strip_name (str, p, sizeof p))
    return 0;

  if ((ob = lookup_object_hash (p)))
    return ob;

  return 0;
}

/*
 * Transfer an object.
 * The object has to be taken from one inventory list and added to another.
 * The main work is to update all command definitions, depending on what is
 * living or not. Note that all objects in the same inventory are affected.
 */
void
move_object (object_t * item, object_t * dest)
{
  object_t **pp, *ob;
  object_t *next_ob;
  object_t *save_cmd = command_giver;

  /* Recursive moves are not allowed. */
  for (ob = dest; ob; ob = ob->super)
    if (ob == item)
      error (_("*Can't move object inside itself."));

#ifdef LAZY_RESETS
  try_reset (dest);
#endif

  if (item->super)
    {
      int okey = 0;

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
	  okey = 1;
	}
    }

  /* link object into target's inventory list */
  item->next_inv = dest->contains;
  dest->contains = item;
  item->super = dest;

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
	error (_("*An object was destructed at call of " APPLY_INIT "()"));

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
	error (_("*The object to be moved was destructed at call of " APPLY_INIT
	       "()!"));

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
    error (_("*The destination to move to was destructed at call of " APPLY_INIT
	   "()!"));

  if (dest->flags & O_ENABLE_COMMANDS)
    {
      command_giver = dest;
      (void) apply (APPLY_INIT, item, 0, ORIGIN_DRIVER);
    }

  command_giver = save_cmd;
}


static sentence_t *sent_free = 0;
int tot_alloc_sentence;

static sentence_t *
alloc_sentence ()
{
  sentence_t *p;

  if (sent_free == 0)
    {
      p = ALLOCATE (sentence_t, TAG_SENTENCE, "alloc_sentence");
      tot_alloc_sentence++;
    }
  else
    {
      p = sent_free;
      sent_free = sent_free->next;
    }
  p->verb = 0;
  p->function.s = 0;
  p->next = 0;
  return p;
}


void
free_sentence (sentence_t * p)
{
  if (p->flags & V_FUNCTION)
    {
      if (p->function.f)
	free_funp (p->function.f);
      p->function.f = 0;
    }
  else
    {
      if (p->function.s)
	free_string (p->function.s);
      p->function.s = 0;
    }

  if (p->verb)
    free_string (p->verb);

  p->verb = 0;
  p->next = sent_free;
  sent_free = p;
}


/*
 * Find the sentence for a command from the user.
 * Return success status.
 */

#define MAX_VERB_BUFF 100

int
user_parser (char *buff)
{
  char verb_buff[MAX_VERB_BUFF];
  object_t *super;
  sentence_t *s;
  char *p;
  int length;
  object_t *save_command_giver = command_giver;
  char *user_verb = 0;
  int where;
  int save_illegal_sentence_action;

  /* 刪除指令列後面多餘的空白字元 */
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
      int pos;

      pos = p - buff;
      if (pos < MAX_VERB_BUFF)
	{
	  verb_buff[pos] = '\0';
	}
    }

  /* 搜尋 command_giver 所有被賦予的 sentence */

  save_illegal_sentence_action = illegal_sentence_action;
  illegal_sentence_action = 0;

  for (s = save_command_giver->sent; s; s = s->next)
    {
      svalue_t *ret;
      object_t *command_object;

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

      /* 找到可能符合的 sentence */

      if (s->flags & V_NOSPACE)
	{
	  int l1 = strlen (s->verb);
	  int l2 = strlen (verb_buff);

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

      /* Remember the object, to update moves. */
      command_object = s->ob;
      super = command_object->super;

      if (s->flags & V_NOSPACE)
	copy_and_push_string (&buff[strlen (s->verb)]);
      else if (buff[length] == ' ')
	copy_and_push_string (&buff[length + 1]);
      else
	push_undefined ();

      /* 呼叫指令處理函式 */
      if (s->flags & V_FUNCTION)
	ret = call_function_pointer (s->function.f, 1);
      else
	{
	  if (s->function.s[0] == APPLY___INIT_SPECIAL_CHAR)
	    error (_("*Illegal function name."));
	  ret = apply (s->function.s, s->ob, 1, where);
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
	      char buf[256];
	      if (s->flags & V_FUNCTION)
		{
		  sprintf (buf,
			   _("*Verb '%s' bound to uncallable function pointer."),
			   s->verb);
		  error (buf);
		}
	      else
		{
		  sprintf (buf, _("*Function for verb '%s' not found."),
			   s->verb);
		  error (buf);
		}
	    }
	}

      /* 指令被成功地辨識並執行 ? */
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
	      error (_
		("*Illegal to call remove_action() from a verb returning zero."));
	    case 2:
	      error (_
		("*Illegal to move or destruct an object defining actions from a verb function which returns zero."));
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
 * The object must be near the command giver, so that we ensure that the
 * sentence is removed when the command giver leaves.
 *
 * If the call is from a shadow, make it look like it is really from
 * the shadowed object.
 */
void
add_action (svalue_t * str, char *cmd, int flag)
{
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
  p->verb = make_shared_string (cmd);
  /* This is ok; adding to the top of the list doesn't harm anything */
  p->next = command_giver->sent;
  command_giver->sent = p;
}


/*
 * Remove sentence with specified verb and action.  Return 1
 * if success.  If command_giver, remove his action, otherwise
 * remove current_object's action.
 */
int
remove_action (char *act, char *verb)
{
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


/*
 * Remove all commands (sentences) defined by object 'ob' in object
 * 'user'
 */
static void
remove_sent (object_t * ob, object_t * user)
{
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

static int
find_line (const char *p, const program_t * progp, char **ret_file, int *ret_line)
{
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

  offset = p - progp->program;
  if (offset > (int) progp->program_size)
    {
      debug_error ("illegal offset %+d in object /%s", offset, progp->name);
      //fatal ("illegal offset %+d in object /%s", offset, progp->name);
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

static void
get_explicit_line_number_info (char *p, program_t * prog, char **ret_file,
			       int *ret_line)
{
  find_line (p, prog, ret_file, ret_line);
  if (!(*ret_file))
    *ret_file = prog->name;
}

static void
get_line_number_info (char **ret_file, int *ret_line)
{
  find_line (pc, current_prog, ret_file, ret_line);
  if (!(*ret_file))
    *ret_file = current_prog->name;
}

static char*
get_line_number (const char *p, const program_t * progp)
{
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

static void inline
get_trace_details (const program_t* prog, int index, function_trace_details_t* ftd)
{
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
char *
dump_trace (int how)
{
  const control_stack_t *p;
  char *ret = 0;
  int num_arg = -1, num_local = -1;
  svalue_t *ptr;
  int i, offset = 0;
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
	  log_message (NULL, "\t\e[33m%s()\e[0m at \e[36m%s\e[0m, in program /%s (object %s)\n", ftd.name,
		       get_line_number (p[1].pc, p[1].prog), p[1].prog->name, p[1].ob->name);
	  if (strcmp (ftd.name, "heart_beat") == 0)
	    ret = p->ob ? p->ob->name : 0;
	  break;
	case FRAME_FUNP:
	  log_message (NULL, "\t\e[33m(function)\e[0m at \e[36m%s\e[0m, in program /%s (object %s)\n",
		       get_line_number (p[1].pc, p[1].prog), p[1].prog->name, p[1].ob->name);
	  num_arg = p[0].fr.funp->f.functional.num_arg;
	  num_local = p[0].fr.funp->f.functional.num_local;
	  break;
	case FRAME_FAKE:
	  log_message (NULL, "\t\e[33m(function)\e[0m at \e[36m%s\e[0m, in program /%s (object %s)\n",
		       get_line_number (p[1].pc, p[1].prog), p[1].prog->name, p[1].ob->name);
	  num_arg = -1;
	  break;
	case FRAME_CATCH:
	  log_message (NULL, "\t\e[33m(catch)\e[0m at \e[36m%s\e[0m, in program /%s (object %s)\n",
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
      offset = ftd.program_offset;
      num_arg = ftd.num_arg;
      num_local = ftd.num_local;
      log_message (NULL, "\t\e[1;33m%s()\e[0m at \e[1;36m%s\e[0m, in program /%s (object %s)\n", ftd.name,
		   get_line_number (pc, current_prog), current_prog->name, current_object->name);
      break;
    case FRAME_FUNP:
      log_message (NULL, "\t\e[1;33m(function)\e[0m at \e[1;36m%s\e[0m, in program /%s (object %s)\n",
		   get_line_number (pc, current_prog), current_prog->name, current_object->name);
      num_arg = p[0].fr.funp->f.functional.num_arg;
      num_local = p[0].fr.funp->f.functional.num_local;
      break;
    case FRAME_FAKE:
      log_message (NULL, "\t\e[1;33m(function)\e[0m at \e[1;36m%s\e[0m, in program /%s (object %s)\n",
		   get_line_number (pc, current_prog), current_prog->name, current_object->name);
      num_arg = -1;
      break;
    case FRAME_CATCH:
      log_message (NULL, "\t\e[1;33m(catch)\e[0m at \e[1;36m%s\e[0m, in program /%s (object %s)\n",
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

  log_message (NULL, "\tdisassembly:\n");
  disassemble (current_log_file, current_prog->program, offset, offset + 30, current_prog);
  fflush (current_log_file);
  return ret;
}

array_t *
get_svalue_trace (int how)
{
  control_stack_t *p;
  array_t *v;
  mapping_t *m;
  char *file;
  int line;
  int num_arg, num_local;
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

void
fatal (char *fmt, ...)
{
  char *msg = "(error message buffer cannot be allocated)";
  va_list args;

  va_start (args, fmt);
  if (-1 == vasprintf (&msg, fmt, args)) {
    debug_message(_("{}\t***** failed to format fatal error message \"%s\"."), fmt);
    exit (EXIT_FAILURE);
  }
  va_end (args);

  debug_message (_("{}\t***** %s"), msg);

  if (proceeding_fatal_error)
    {
      debug_message (_("{}\t***** fatal error occured while another proceeding, shutdown immediately."));
    }
  else
    {
      char *ob_name;
      error_context_t econ;

      proceeding_fatal_error = 1;

      if (current_file)
	debug_message (_("{}\t----- compiling %s at line %d"), current_file, current_line);

      if (current_object)
	debug_message (_("{}\t----- current object was /%s"), current_object->name);

      if ((ob_name = dump_trace (DUMP_WITH_ARGS | DUMP_WITH_LOCALVARS)))
	debug_message (_("{}\t----- in heart beat of /%s"), ob_name);

      save_context (&econ);
      if (setjmp (econ.context))
	{
	  debug_message (_("{}\t***** error in master::%s(), shutdown now."), APPLY_CRASH);
	}
      else
	{
	  copy_and_push_string (msg);
	  if (command_giver)
	    push_object (command_giver);
	  else
	    push_undefined ();

	  if (current_object)
	    push_object (current_object);
	  else
	    push_undefined ();

	  apply_master_ob (APPLY_CRASH, 3);
	  debug_message (_("{}\t----- mudlib crash handler finished, shutdown now."));
	}
    }

  free (msg);

  /* 緊急結束程式 */
  if (CONFIG_INT (__ENABLE_CRASH_DROP_CORE__))
    abort ();
  else
    exit (EXIT_FAILURE);
}

static volatile int in_error = 0;
static volatile int in_mudlib_error_handler = 0;

/*
 * Error() has been "fixed" so that users can catch and throw them.
 * To catch them nicely, we really have to provide decent error information.
 * Hence, all errors that are to be caught
 * (error_recovery_context_exists == 2) construct a string containing
 * the error message, which is returned as the
 * thrown value.  Users can throw their own error values however they choose.
 */

/*
 * This is here because throw constructs its own return value; we dont
 * want to replace it with the system's error string.
 */

void
throw_error ()
{
  if (((current_error_context->save_csp + 1)->framekind & FRAME_MASK) ==
      FRAME_CATCH)
    {
      longjmp (current_error_context->context, 1);
      fatal ("Failed longjmp() in throw_error()!");
    }
  error (_("*Throw with no catch."));
}

static void
debug_message_with_location (char *err)
{
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


static void
mudlib_error_handler (char *err, int catch)
{
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
  if (catch)
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

void
error_handler (char *err)
{
  /* in case we're going to jump out of load_object */
  restrict_destruct = 0;
  num_objects_this_thread = 0;	/* reset the count */

  /* 錯誤是否被 LPC 程式碼 catch ? */
  if (((current_error_context->save_csp + 1)->framekind & FRAME_MASK)
      == FRAME_CATCH && !proceeding_fatal_error)
    {
#ifdef LOG_CATCHES
      /* 允許被 catch 的錯誤也經過 master::error_handler() 處理 */
      if (in_mudlib_error_handler)
	{
	  debug_message (_("{}\t***** error in mudlib error handler (caught)"));
	  debug_message_with_location (err);
	  dump_trace (g_trace_flag);
	  in_mudlib_error_handler = 0;
	}
      else
	{
	  /* 呼叫 master::error_handler() */
	  in_mudlib_error_handler = 1;
	  mudlib_error_handler (err, 1);
	  in_mudlib_error_handler = 0;
	}
#endif	/* LOG)CATCHES */

      /* free catch_value allocated in last catch if any */
      free_svalue (&catch_value, "caught error");

      /* allocate new catch_value */
      catch_value.type = T_STRING;
      catch_value.subtype = STRING_MALLOC;
      catch_value.u.string = string_copy (err, "caught error");

      /* jump to do_catch */
      longjmp (current_error_context->context, 1);
      fatal ("catch() longjump failed");
    }

  /* 錯誤未被 catch, 循正常管道處理 */

  if (in_error)
    {
      debug_message (_("{}\t***** New error occured while generating error trace!"));
      debug_message_with_location (err);
      dump_trace (g_trace_flag);

      if (current_error_context)
	longjmp (current_error_context->context, 1);
      fatal ("failed longjmp() or no error context for error.");
    }

  /* 開始 error 處理 */
  in_error = 1;

  if (in_mudlib_error_handler)
    {
      debug_message (_("{}\t***** error in mudlib error handler"));
      debug_message_with_location (err);
      dump_trace (g_trace_flag);
      in_mudlib_error_handler = 0;
    }
  else
    {
      /* 準備呼叫 master::error_handler */
      in_mudlib_error_handler = 1;
      in_error = 0;	/* 暫時隱藏錯誤處理狀態 */
      mudlib_error_handler (err, 0);
      in_error = 1;	/* 回復到錯誤處理狀態 */
      in_mudlib_error_handler = 0;
    }

  /* 若 error 發生在在 heart_beat 的執行過程, 關閉該物件的 heart_beat */
  if (current_heart_beat)
    {
      set_heart_beat (current_heart_beat, 0);
      debug_message (_("{}\t----- heart beat in %s turned off\n"),
		     current_heart_beat->name);
#if 0
      if (current_heart_beat->interactive)
	add_message (current_heart_beat, _("Your heart beat stops!\n"));
#endif
      current_heart_beat = 0;
    }

  /* 結束 error 處理, 繼續執行 */
  in_error = 0;

  if (current_error_context)
    longjmp (current_error_context->context, 1);
  fatal (_("failed longjmp() or no error context for error."));
}

void
error (char *fmt, ...)
{
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


/*
 * This one is called from the command "shutdown".
 * We don't call it directly from HUP, because it is dangerous when being
 * in an interrupt.
 */
void
do_shutdown (int exit_code)
{
  int i;

  shout_string ("Shutting down immediately.\n");
  ipc_remove ();
  for (i = 0; i < max_lpc_socks; i++)
    {
      if (lpc_socks[i].state == CLOSED)
	continue;
      while (close (lpc_socks[i].fd) == -1 && errno == EINTR)
	;
    }
  for (i = 0; i < max_users; i++)
    {
      if (all_users[i] && !(all_users[i]->iflags & CLOSING))
	flush_message (all_users[i]);
    }

#ifdef PROFILING
  monitor (0, 0, 0, 0, 0);	/* cause gmon.out to be written */
#endif
  exit (exit_code);
}

/*
 * Call this one when there is only little memory left. It will start
 * Armageddon.
 */
void
slow_shut_down (int minutes)
{
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


void
do_message (svalue_t * class, svalue_t * msg, array_t * scope,
	    array_t * exclude, int recurse)
{
  int i, j, valid;
  object_t *ob;

  for (i = 0; i < scope->size; i++)
    {
      switch (scope->item[i].type)
	{
	case T_STRING:
	  ob = find_object (scope->item[i].u.string);
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
	      push_svalue (class);
	      push_svalue (msg);
	      apply (APPLY_RECEIVE_MESSAGE, ob, 2, ORIGIN_DRIVER);
	    }
	}
      else if (recurse)
	{
	  array_t *tmp;

	  tmp = all_inventory (ob, 1);
	  do_message (class, msg, tmp, exclude, 0);
	  free_array (tmp);
	}
    }
}


#ifdef LAZY_RESETS
void
try_reset (object_t * ob)
{
  if ((ob->next_reset < current_time) && !(ob->flags & O_RESET_STATE))
    {
      /* need to set the flag here to prevent infinite loops in apply_low */
      ob->flags |= O_RESET_STATE;
      reset_object (ob);
    }
}
#endif


#ifdef F_FIRST_INVENTORY
object_t *
first_inventory (svalue_t * arg)
{
  object_t *ob;

  if (arg->type == T_STRING)
    {
      ob = find_object (arg->u.string);
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


char *
origin_name (int orig)
{
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
