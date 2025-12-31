#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

/* 92/04/18 - cleaned up stylistically by Sulam@TMI */

#include "std.h"
#include "lpc/types.h"
#include "lpc/array.h"
#include "lpc/object.h"
#include "lpc/program.h"
#include "lpc/include/origin.h"
#include "lpc/include/runtime_config.h"
#include "rc.h"
#include "interpret.h"
#include "backend.h"
#include "comm.h"
#include "efuns/call_out.h"

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

/* The 'current_time' is updated at every heart beat. */
time_t current_time = 0;

int heart_beat_flag = 0;

object_t *current_heart_beat;

#ifdef HAVE_LIBRT
static timer_t hb_timerid = 0; /* heart beat timer id */
#endif

size_t eval_cost = 0;

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
static void clear_state () {
  current_object = 0;
  command_giver = 0;
  current_interactive = 0;
  previous_ob = 0;
  current_prog = 0;
  caller_type = 0;
  reset_interpreter ();		/* Pop down the stack. */
}

/**
 *  @brief This function calls the master object's connect() function to
 *  determine whether to accept a new user connection. If accepted, the
 *  user object returned by connect() is initialized and returned.
 *  @param port The port number on which the new connection was received.
 *  @param addr The address of the connecting user as a string.
 *  @return The user object if the connection is accepted, NULL otherwise.
 */
object_t* mudlib_connect(int port, const char* addr) {

  object_t *ob;
  svalue_t *ret;
  /*
   * The user object has one extra reference. It is asserted that the
   * master_ob is loaded.
   */
  add_ref (master_ob, "mudlib_connect");
  push_number (port);
  ret = apply_master_ob (APPLY_CONNECT, 1);
  /* master_ob->interactive can be zero if the master object self destructed in the above. */
  if (ret == 0 || ret == (svalue_t *) - 1 || ret->type != T_OBJECT || !master_ob->interactive)
    {
      debug_message ("connection from %s rejected by master\n", addr);
      return 0;
    }

  /*
   * There was an object returned from connect(). Use this as the user object.
   */
  ob = ret->u.ob;
  if (ob->flags & O_HIDDEN)
    num_hidden++;
  ob->interactive = master_ob->interactive;
  ob->interactive->ob = ob;
  ob->flags |= O_ONCE_INTERACTIVE;
  if (ob->interactive->fd == STDIN_FILENO)
    ob->flags |= O_CONSOLE_USER; /* mark as console user */
  /*
   * assume the existance of write_prompt and process_input in user.c
   * until proven wrong (after trying to call them).
   */
  ob->interactive->iflags |= (HAS_WRITE_PROMPT | HAS_PROCESS_INPUT);

  master_ob->flags &= ~(O_ONCE_INTERACTIVE|O_CONSOLE_USER);
  master_ob->interactive = 0;
  free_object (master_ob, "mudlib_connect"); /* remove extra reference added when calling connect() */
  add_ref (ob, "mudlib_connect");
  command_giver = ob;
  return ob;
}

void
mudlib_logon (object_t * ob)
{
  /* current_object no longer set */
  apply (APPLY_LOGON, ob, 0, ORIGIN_DRIVER);
  /* function not existing is no longer fatal */
}

/**
 *  @brief Initiate connection of the console user in console mode.
 *  A console user is a special interactive user that uses the standard
 *  input/output of the driver process as the communication channel.
 *
 *  When a console user is disconnected, the stdin is not closed, and
 *  this function can be called again to reconnect the console user.
 *
 *  This is a Neolith extension.
 */
void init_console_user(int reconnect) {

  object_t* ob;
  new_interactive(STDIN_FILENO);
  master_ob->interactive->connection_type = PORT_TELNET; /* PORT_CONSOLE */
  master_ob->interactive->addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  eval_cost = CONFIG_INT (__MAX_EVAL_COST__);
  ob = mudlib_connect(0, "console"); /* port 0 for console */
  if (!ob)
    {
      if (master_ob->interactive)
        remove_interactive (master_ob, 0);
      return;
    }
#ifdef HAVE_TERMIOS_H
  {
    /* enable canonical mode and echo, in case console user were disconnected while typing */
    struct termios tio;

    tcgetattr (STDIN_FILENO, &tio);
    tio.c_lflag |= ICANON | ECHO;
    tio.c_cc[VMIN] = 0; /* use polling as like O_NONBLOCK was set */
    tio.c_cc[VTIME] = 0; /* no timeout */
    tcsetattr (STDIN_FILENO, TCSAFLUSH, &tio); /* discard pending input */
  }
#elif defined(WINSOCK)
  {
    /* Enable Windows's vt100 simulation with high-level console input and output modes.
     * This allows ANSI escape sequences for text color and cursor movement to work on stdout.
     * Reference: https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
     */
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    SetConsoleCP(CP_UTF8);
    GetConsoleMode(hStdin, &mode);
    SetConsoleMode(hStdin, mode | ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    SetConsoleOutputCP(CP_UTF8);
    GetConsoleMode(hStdout, &mode);
    SetConsoleMode(hStdout, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  }
#endif
  if (reconnect)
    {
      /* Any pending input and the ENTER key was discarded after calling tcssetattr() with TCSAFLUSH */
      debug_message("Console user re-connected.\n");
    }
  mudlib_logon(ob);
}

/*  backend()
 */
void backend () {

  struct timeval timeout;
  int nb;
  int i;
  error_context_t econ;

  opt_info (1, "Entering backend loop.");

#ifdef HAVE_LIBRT
  if (-1 == timer_create (CLOCK_REALTIME, NULL, &hb_timerid))
    {
      debug_perror ("timer_create()", NULL);
      return;
    }
#else
  opt_warn (0, "Timer functions not available, heart_beat(), call_out() and reset() disabled.");
#endif /* HAVE_LIBRT */

#ifdef WINSOCK
  {
    // Initialize Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        debug_fatal("WSAStartup() failed: %d\n", iResult);
        exit(EXIT_FAILURE);
    }
  }
#endif
  init_user_conn ();		/* initialize user connection socket */

  if (!t_flag)
    {
#ifdef HAVE_LIBRT
      sigset_t set;
      sigemptyset (&set);
      sigaddset (&set, SIGALRM);
      sigprocmask (SIG_UNBLOCK, &set, NULL);
#endif
      call_heart_beat ();
    }
  clear_state ();
  save_context (&econ);

  if (setjmp (econ.context))
    restore_context (&econ);

  if (MAIN_OPTION(console_mode))
    init_console_user(0);

  while (1)
    {
      /* Has to be cleared if we jumped out of process_user_command() */
      current_interactive = 0;
      eval_cost = CONFIG_INT (__MAX_EVAL_COST__);

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

      if (heart_beat_flag)
        {
          timeout.tv_sec = 0;
          timeout.tv_usec = 0;
        }
      else
        {
          timeout.tv_sec = 60;
          timeout.tv_usec = 0;
        }
      nb = do_comm_polling (&timeout);
      if (nb == SOCKET_ERROR)
        {
          /* FIXME: STDIN_FILENO in fdset causes WSAENOTSOCK error in Winsock */
          opt_trace(TT_BACKEND|4, "select() failed with error: %d\n", SOCKET_ERRNO);
        }
      opt_trace (TT_BACKEND|4, "do_comm_polling returned %d", nb);

      if (nb > 0)
        process_io ();

      for (i = 0; process_user_command () && i < max_users; i++);

      if (heart_beat_flag)
        call_heart_beat ();
    }
  pop_context (&econ);

#ifdef WINSOCK
  WSACleanup(); /* for graceful shutdown */
#endif
}

/**
 *  @brief Despite the name, this routine takes care of several things.
 *      It will loop through all objects once every 15 minutes.
 *
 *      If an object is found in a state of not having done reset, and the
 *      delay to next reset has passed, then reset() will be done.
 *
 *      If the object has a existed more than the time limit given for swapping,
 *      then 'clean_up' will first be called in the object, after which it will
 *      be swapped out if it still exists.
 *
 *      There are some problems if the object self-destructs in clean_up, so
 *      special care has to be taken of how the linked list is used.
 */
static void look_for_objects_to_swap () {
  static time_t next_time;
  object_t* next_ob;
  object_t *ob;
  error_context_t econ;

  if (current_time < next_time)	/* Not time to look yet */
    return;
  next_time = current_time + 15 * 60;	/* Next time is in 15 minutes */


  save_context (&econ);
  if (setjmp (econ.context))
    restore_context (&econ); /* catch errors in reset() or clean_up() */

  for (ob = obj_list; ob; ob = next_ob)
    {
      int ref_time;

      /*
       * Objects can be destructed, which means that next object to
       * look at (saved in next_ob) may be removed from obj_list.
       * In that case, the loop is simply restarted.
       */
      if (ob->flags & O_DESTRUCTED)
        {
          ob = obj_list;
          if (!ob)
            break;
        }
      next_ob = ob->next_all;
      ref_time = ob->time_of_ref;
      eval_cost = CONFIG_INT (__MAX_EVAL_COST__);

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

          if (current_time - ref_time > CONFIG_INT (__TIME_TO_CLEAN_UP__)
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
#ifdef HAVE_LIBRT
  struct itimerspec itimer;
#endif /* HAVE_LIBRT */

  heart_beat_flag = 0;

#ifdef HAVE_LIBRT
  signal (SIGALRM, sigalrm_handler);
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
  opt_trace (TT_BACKEND|1, "current_time: %ul, num_hb_objs: %d", current_time, num_hb_objs);
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
                  opt_trace (TT_BACKEND|2, "total: %d/%d, current: %s", heart_beat_index + 1, num_hb_to_do, ob->name);
                  call_function (ob->prog, ob->prog->heart_beat, 0, 0);
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
  look_for_objects_to_swap (); /* check for LPC object reset() */
  call_out (); /* check for LPC call_out() */
}

int query_heart_beat (object_t * ob) {
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

/**
 * @brief Add or remove an object from the heart beat list; does the major check...
 * If an object removes something from the list from within a heart beat,
 * various pointers in call_heart_beat could be stuffed, so we must
 * check current_heart_beat and adjust pointers.
 */

int set_heart_beat (object_t * ob, int to) {
  int index;

  if (ob->flags & O_DESTRUCTED)
    return 0;

  if (!to)
    {
      /* remove from heart beat list */
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
        memmove (heart_beats + index, heart_beats + (index + 1), num * sizeof (heart_beat_t));

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
      DEBUG_CHECK (index < 0, "Couldn't find enabled object in heart_beat list!\n");
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

/**
  * @brief New version used when not in -o mode. The epilog() in master.c is
  *     supposed to return an array of files (castles in 2.4.5) to load. The array
  *     returned by apply() will be freed at next call of apply(), which means that
  *     the ref count has to be incremented to protect against deallocation.
  *
  *     The master object is asked to do the actual loading.
  */
void preload_objects (int eflag) {

  array_t *prefiles;
  svalue_t *ret;
  volatile int ix;
  error_context_t econ;

  save_context (&econ);
  if (setjmp (econ.context))
    {
      restore_context (&econ); /* catch errors in master apply epilog() */
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
  opt_info (1, "Preloading %d objects", prefiles->size);

  prefiles->ref++;
  ix = 0;

  /* in case of an error, effectively do a 'continue' */
  save_context (&econ);
  if (setjmp (econ.context))
    {
      restore_context (&econ); /* catch errors in master apply preload() */
      opt_warn (1, "Error preloading file %d/%d, continuing.", ix + 1, prefiles->size);
      ix++;
    }
  for (; ix < prefiles->size; ix++)
    {
      if (prefiles->item[ix].type != T_STRING)
        continue;
      eval_cost = CONFIG_INT (__MAX_EVAL_COST__);   /* prevents infinite loops */
      push_svalue (prefiles->item + ix);
      (void) apply_master_ob (APPLY_PRELOAD, 1);
    }
  free_array (prefiles);
  pop_context (&econ);

  opt_info (1, "Preloading complete");
}				/* preload_objects() */

#define NUM_CONSTS 5
static double consts[NUM_CONSTS];

void
init_precomputed_tables ()
{
  int i;

  for (i = 0; i < (int)(sizeof consts / sizeof consts[0]); i++)
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

  sprintf (buff, "%.2f cmds/s, %.2f comp lines/s", load_av, compile_av);
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
  opt_trace (TT_BACKEND|2, "SIGALRM");
}
