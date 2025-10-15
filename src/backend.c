#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

/* 92/04/18 - cleaned up stylistically by Sulam@TMI */

#include "std.h"
#include "lpc/types.h"
#include "lpc/array.h"
#include "lpc/object.h"
#include "program.h"
#include "lpc/include/origin.h"
#include "lpc/include/runtime_config.h"
#include "main.h"
#include "rc.h"
#include "simulate.h"
#include "interpret.h"
#include "backend.h"
#include "comm.h"
#include "efuns/replace_program.h"
#include "efuns/call_out.h"

error_context_t *current_error_context = 0;

/* The 'current_time' is updated at every heart beat. */
time_t current_time;

int heart_beat_flag = 0;

object_t *current_heart_beat;

static timer_t hb_timerid = 0; /* heart beat timer id */

static void look_for_objects_to_swap (void);
static void call_heart_beat (void);

static RETSIGTYPE sigalrm_handler (int);

/*
 * There are global variables that must be zeroed before any execution.
 * In case of errors, there will be a LONGJMP(), and the variables will
 * have to be cleared explicitely. They are normally maintained by the
 * code that use them.
 *
 * This routine must only be called from top level, not from inside
 * stack machine execution (as stack will be cleared).
 */
void
clear_state ()
{
  current_object = 0;
  command_giver = 0;
  current_interactive = 0;
  previous_ob = 0;
  current_prog = 0;
  caller_type = 0;
  reset_machine ();		/* Pop down the stack. */
}				/* clear_state() */

void
logon (object_t * ob)
{
  /* current_object no longer set */
  apply (APPLY_LOGON, ob, 0, ORIGIN_DRIVER);
  /* function not existing is no longer fatal */
}

/*
 * Take a user command and parse it.
 * The command can also come from a NPC.
 * Beware that 'str' can be modified and extended !
 */
int
parse_command (char *str, object_t * ob)
{
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
}				/* parse_command() */

/*  backend()
 * 
 *  �o�Ө�ƬO�t�Ϊ��D�j��A�����Өt�Ϊ� I/O polling �M update�C
 */
int eval_cost;

void
backend ()
{
  struct timeval timeout;
  int nb;
  int i;
  error_context_t econ;

#ifdef HAVE_LIBRT
  if (-1 == timer_create (CLOCK_REALTIME, NULL, &hb_timerid))
    {
      debug_perror ("timer_create()", NULL);
      return;
    }
#endif /* HAVE_LIBRT */
  init_user_conn ();		/* initialize user connection socket */

  if (!t_flag)
    {
      sigset_t set;
      sigemptyset (&set);
      sigaddset (&set, SIGALRM);
      sigprocmask (SIG_UNBLOCK, &set, NULL);
      call_heart_beat ();
    }
  clear_state ();
  save_context (&econ);

  if (setjmp (econ.context))
    restore_context (&econ);

  while (1)
    {
      /* Has to be cleared if we jumped out of process_user_command() */
      current_interactive = 0;
      eval_cost = CONFIG_INT (__MAX_EVAL_COST__);

      if (obj_list_replace || obj_list_destruct)
	remove_destructed_objects ();

      /*
       * do shutdown if g_proceeding_shutdown is set
       */
      if (g_proceeding_shutdown)
	do_shutdown (0);

      if (slow_shut_down_to_do)
	{
	  int tmp = slow_shut_down_to_do;

	  slow_shut_down_to_do = 0;
	  slow_shut_down (tmp);
	}

      /* �ˬd�s�u�ϥΪ̪� I/O �ƥ� */
      make_selectmasks ();
      if (heart_beat_flag)
	{
	  /* �p�G�٦� heart_beat �S�������A�� timeout �]�w�� 0�A�ҥH
	   * select ���|���� I/O ���o�ͦӥߧY��^�A�H�K�ڭ̳B�z�|��
	   * ������ heart_beat
	   */
	  timeout.tv_sec = 0;
	  timeout.tv_usec = 0;
	}
      else
	{
	  timeout.tv_sec = 60;
	  timeout.tv_usec = 0;
	}
      nb = get_IO_polling (&timeout);

      /* �p�G�� socket �� I/O �o�͡A�B�z�o�� I/O */
      if (nb > 0)
	process_io ();

      /* �B�z�s�u�ϥΪ̪����O */
      for (i = 0; process_user_command () && i < max_users; i++);

      /* �B�z heart_beat �u�@ */
      if (heart_beat_flag)
	call_heart_beat ();
    }
}

/*
 * Despite the name, this routine takes care of several things.
 * It will loop through all objects once every 15 minutes.
 *
 * If an object is found in a state of not having done reset, and the
 * delay to next reset has passed, then reset() will be done.
 *
 * If the object has a existed more than the time limit given for swapping,
 * then 'clean_up' will first be called in the object, after which it will
 * be swapped out if it still exists.
 *
 * There are some problems if the object self-destructs in clean_up, so
 * special care has to be taken of how the linked list is used.
*/
static void
look_for_objects_to_swap ()
{
  static time_t next_time;
  object_t *ob;
  static object_t *next_ob;
  error_context_t econ;

  if (current_time < next_time)	/* Not time to look yet */
    return;
  next_time = current_time + 15 * 60;	/* Next time is in 15 minutes */

  /*
   * Objects can be destructed, which means that next object to
   * look at is saved in next_ob. If very unlucky, that object can be
   * destructed too. In that case, the loop is simply restarted.
   */
  next_ob = obj_list;

  /* �Y�o�Ϳ��~�A�| longjump �^��o�̭��s�}�l */
  save_context (&econ);
  if (setjmp (econ.context))
    restore_context (&econ);

  while ((ob = (object_t *) next_ob))
    {
      int ref_time = ob->time_of_ref;

      eval_cost = CONFIG_INT (__MAX_EVAL_COST__);

      if (ob->flags & O_DESTRUCTED)
	ob = obj_list;		/* restart */
      next_ob = ob->next_all;

      /*
       * Check reference time before reset() is called.
       */

#ifndef LAZY_RESETS
      /* Should this object have reset(1) called ? */
      if ((ob->flags & O_WILL_RESET) && (ob->next_reset < current_time)
	  && !(ob->flags & O_RESET_STATE))
	{
	  reset_object (ob);
	}
#endif

      if (CONFIG_INT (__TIME_TO_CLEAN_UP__) > 0)
	{
	  /*
	   * Has enough time passed, to give the object a chance to
	   * self-destruct ? Save the O_RESET_STATE, which will be cleared.
	   * 
	   * Only call clean_up in objects that has defined such a function.
	   * 
	   * Only if the clean_up returns a non-zero value, will it be called
	   * again.
	   */

	  if (current_time - ref_time >
	      CONFIG_INT (__TIME_TO_CLEAN_UP__)
	      && (ob->flags & O_WILL_CLEAN_UP))
	    {
	      int save_reset_state = ob->flags & O_RESET_STATE;
	      svalue_t *svp;

	      /*
	       * Supply a flag to the object that says if this program is
	       * inherited by other objects. Cloned objects might as well
	       * believe they are not inherited. Swapped objects will not
	       * have a ref count > 1 (and will have an invalid ob->prog
	       * pointer).
	       */

	      push_number ((ob->flags & O_CLONE) ? 0 : ob->prog->ref);
	      svp = apply (APPLY_CLEAN_UP, ob, 1, ORIGIN_DRIVER);
	      if (ob->flags & O_DESTRUCTED)
		continue;
	      if (!svp || (svp->type == T_NUMBER && svp->u.number == 0))
		ob->flags &= ~O_WILL_CLEAN_UP;
	      ob->flags |= save_reset_state;
	    }
	}
    }

  pop_context (&econ);
}				/* look_for_objects_to_swap() */

/* Call all heart_beat() functions in all objects.  Also call the next reset,
 * and the call out.
 * We do heart beats by moving each object done to the end of the heart beat
 * list before we call its function, and always using the item at the head
 * of the list as our function to call.  We keep calling heart beats until
 * a timeout or we have done num_heart_objs calls.  It is done this way so
 * that objects can delete heart beating objects from the list from within
 * their heart beat without truncating the current round of heart beats.
 *
 * Set command_giver to current_object if it is a living object. If the object
 * is shadowed, check the shadowed object if living. There is no need to save
 * the value of the command_giver, as the caller resets it to 0 anyway.  */

typedef struct
{
  object_t *ob;
  short heart_beat_ticks;
  short time_to_heart_beat;
}
heart_beat_t;

static heart_beat_t *heart_beats = 0;
static int max_heart_beats = 0;
static int heart_beat_index = 0;
static int num_hb_objs = 0;
static int num_hb_to_do = 0;

static int num_hb_calls = 0;	/* starts */
static float perc_hb_probes = 100.0;	/* decaying avge of how many complete */

static void
call_heart_beat ()
{
  object_t *ob;
  heart_beat_t *curr_hb;

  heart_beat_flag = 0;
  signal (SIGALRM, sigalrm_handler);

#ifdef HAVE_LIBRT
  struct itimerspec itimer;
  itimer.it_interval.tv_sec = HEARTBEAT_INTERVAL / 1000000;
  itimer.it_interval.tv_nsec = (HEARTBEAT_INTERVAL % 1000000) * 1000;
  itimer.it_value.tv_sec = HEARTBEAT_INTERVAL / 1000000;
  itimer.it_value.tv_nsec = (HEARTBEAT_INTERVAL % 1000000) * 1000;
  if (-1 == timer_settime (hb_timerid, 0, &itimer, NULL))
    {
      debug_perror ("timer_settime()", NULL);
      return;
    }
#endif /* HAVE_LIBRT */
//#ifdef HAVE_UALARM
//  ualarm (HEARTBEAT_INTERVAL, 0);
//#else /* ! HAVE_UALARM */
//  alarm (((HEARTBEAT_INTERVAL + 999999) / 1000000));
//#endif /* ! HAVE_UALARM */

  current_time = time (NULL);
  opt_trace (TT_BACKEND|3, "current_time = %ul", current_time);
  current_interactive = 0;

  if ((num_hb_to_do = num_hb_objs))
    {
      num_hb_calls++;
      heart_beat_index = 0;
      while (!heart_beat_flag)
	{
	  ob = (curr_hb = &heart_beats[heart_beat_index])->ob;
	  /* is it time to do a heart beat ? */
	  curr_hb->heart_beat_ticks--;

	  if (ob->prog->heart_beat != -1)
	    {
	      if (curr_hb->heart_beat_ticks < 1)
		{
		  curr_hb->heart_beat_ticks = curr_hb->time_to_heart_beat;
		  current_heart_beat = ob;
		  command_giver = ob;
		  if (!(command_giver->flags & O_ENABLE_COMMANDS))
		    command_giver = 0;
		  eval_cost = CONFIG_INT (__MAX_EVAL_COST__);
		  /* this should be looked at ... */
		  call_function (ob->prog, ob->prog->heart_beat);
		  command_giver = 0;
		  current_object = 0;
		}
	    }
	  if (++heart_beat_index == num_hb_to_do)
	    break;
	}
      if (heart_beat_index < num_hb_to_do)
	perc_hb_probes = 100 * (float) heart_beat_index / num_hb_to_do;
      else
	perc_hb_probes = 100.0;
      heart_beat_index = num_hb_to_do = 0;
    }
  current_prog = 0;
  current_heart_beat = 0;
  look_for_objects_to_swap ();
  call_out ();
}				/* call_heart_beat() */

int
query_heart_beat (object_t * ob)
{
  int index;

  if (!(ob->flags & O_HEART_BEAT))
    return 0;
  index = num_hb_objs;
  while (index--)
    {
      if (heart_beats[index].ob == ob)
	return heart_beats[index].time_to_heart_beat;
    }
  return 0;
}				/* query_heart_beat() */

/* add or remove an object from the heart beat list; does the major check...
 * If an object removes something from the list from within a heart beat,
 * various pointers in call_heart_beat could be stuffed, so we must
 * check current_heart_beat and adjust pointers.  */

int
set_heart_beat (object_t * ob, int to)
{
  int index;

  if (ob->flags & O_DESTRUCTED)
    return 0;

  if (!to)
    {
      int num;

      index = num_hb_objs;
      while (index--)
	{
	  if (heart_beats[index].ob == ob)
	    break;
	}
      if (index < 0)
	return 0;

      if (num_hb_to_do)
	{
	  if (index <= heart_beat_index)
	    heart_beat_index--;
	  if (index < num_hb_to_do)
	    num_hb_to_do--;
	}

      if ((num = (num_hb_objs - (index + 1))))
	memmove (heart_beats + index, heart_beats + (index + 1),
		 num * sizeof (heart_beat_t));

      num_hb_objs--;
      ob->flags &= ~O_HEART_BEAT;
      return 1;
    }

  if (ob->flags & O_HEART_BEAT)
    {
      if (to < 0)
	return 0;

      index = num_hb_objs;
      while (index--)
	{
	  if (heart_beats[index].ob == ob)
	    {
	      heart_beats[index].time_to_heart_beat =
		heart_beats[index].heart_beat_ticks = to;
	      break;
	    }
	}
      DEBUG_CHECK (index < 0,
		   "Couldn't find enabled object in heart_beat list!\n");
    }
  else
    {
      heart_beat_t *hb;

      if (!max_heart_beats)
	heart_beats = CALLOCATE (max_heart_beats = HEART_BEAT_CHUNK,
				 heart_beat_t, TAG_HEART_BEAT,
				 "set_heart_beat: 1");
      else if (num_hb_objs == max_heart_beats)
	{
	  max_heart_beats += HEART_BEAT_CHUNK;
	  heart_beats = RESIZE (heart_beats, max_heart_beats,
				heart_beat_t, TAG_HEART_BEAT,
				"set_heart_beat: 1");
	}

      hb = &heart_beats[num_hb_objs++];
      hb->ob = ob;
      if (to < 0)
	to = 1;
      hb->time_to_heart_beat = to;
      hb->heart_beat_ticks = to;
      ob->flags |= O_HEART_BEAT;
    }

  return 1;
}

int
heart_beat_status (outbuffer_t * ob, int verbose)
{
  char buf[20];

  if (verbose == 1)
    {
      outbuf_add (ob, "Heart beat information:\n");
      outbuf_add (ob, "-----------------------\n");
      outbuf_addv (ob, "Number of objects with heart beat: %d, starts: %d\n",
		   num_hb_objs, num_hb_calls);

      /* passing floats to varargs isn't highly portable so let sprintf
         handle it */
      sprintf (buf, "%.2f", perc_hb_probes);
      outbuf_addv (ob, "Percentage of HB calls completed last time: %s\n",
		   buf);
    }
  return (0);
}				/* heart_beat_status() */

/* New version used when not in -o mode. The epilog() in master.c is
 * supposed to return an array of files (castles in 2.4.5) to load. The array
 * returned by apply() will be freed at next call of apply(), which means that
 * the ref count has to be incremented to protect against deallocation.
 *
 * The master object is asked to do the actual loading.
 */
void
preload_objects (int eflag)
{
  array_t *prefiles;
  svalue_t *ret;
  volatile int ix;
  error_context_t econ;

  save_context (&econ);
  if (setjmp (econ.context))
    {
      restore_context (&econ);
      pop_context (&econ);
      return;
    }

  push_number (eflag);
  ret = apply_master_ob (APPLY_EPILOG, 1);
  pop_context (&econ);
  if ((ret == 0) || (ret == (svalue_t *) - 1) || (ret->type != T_ARRAY))
    return;
  else
    prefiles = ret->u.arr;
  if ((prefiles == 0) || (prefiles->size < 1))
    return;

  prefiles->ref++;
  ix = 0;

  /* in case of an error, effectively do a 'continue' */
  save_context (&econ);
  if (setjmp (econ.context))
    {
      restore_context (&econ);
      ix++;
    }
  for (; ix < prefiles->size; ix++)
    {
      if (prefiles->item[ix].type != T_STRING)
	continue;

      eval_cost = CONFIG_INT (__MAX_EVAL_COST__);
      /* debug_message(_("preloading %s\n"), prefiles->item[ix].u.string); */

      push_svalue (prefiles->item + ix);
      (void) apply_master_ob (APPLY_PRELOAD, 1);
    }
  free_array (prefiles);
  pop_context (&econ);
}				/* preload_objects() */

/* All destructed objects are moved into a sperate linked list,
 * and deallocated after program execution.  */

inline void
remove_destructed_objects ()
{
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

#define NUM_CONSTS 5
static double consts[NUM_CONSTS];

void
init_precomputed_tables ()
{
  int i;

  for (i = 0; i < sizeof consts / sizeof consts[0]; i++)
    consts[i] = exp (-i / 900.0);
}

static double load_av = 0.0;

void
update_load_av ()
{
  static time_t last_time;
  int n;
  double c;
  static int acc = 0;

  acc++;
  if (current_time == last_time)
    return;
  n = current_time - last_time;
  if (n < NUM_CONSTS)
    c = consts[n];
  else
    c = exp (-n / 900.0);
  load_av = c * load_av + acc * (1 - c) / n;
  last_time = current_time;
  acc = 0;
}				/* update_load_av() */

static double compile_av = 0.0;

void
update_compile_av (int lines)
{
  static time_t last_time;
  int n;
  double c;
  static int acc = 0;

  acc += lines;
  if (current_time == last_time)
    return;
  n = current_time - last_time;
  if (n < NUM_CONSTS)
    c = consts[n];
  else
    c = exp (-n / 900.0);
  compile_av = c * compile_av + acc * (1 - c) / n;
  last_time = current_time;
  acc = 0;
}				/* update_compile_av() */

char *
query_load_av ()
{
  static char buff[100];

  sprintf (buff, _("%.2f cmds/s, %.2f comp lines/s"), load_av, compile_av);
  return (buff);
}				/* query_load_av() */

#ifdef F_HEART_BEATS
array_t *
get_heart_beats ()
{
  int n = num_hb_objs;
  heart_beat_t *hb = heart_beats;
  array_t *arr;

  arr = allocate_empty_array (n);
  while (n--)
    {
      arr->item[n].type = T_OBJECT;
      arr->item[n].u.ob = hb->ob;
      add_ref (hb->ob, "get_heart_beats");
      hb++;
    }
  return arr;
}
#endif


/*
 * SIGALRM handler.
 */
static RETSIGTYPE
sigalrm_handler (int sig)
{
  (void) sig; /* unused */
  heart_beat_flag = 1;
  opt_trace (TT_BACKEND|3, "SIGALRM");
}
