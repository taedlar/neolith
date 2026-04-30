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
#include "command.h"
#include "simul_efun.h"
#include "efuns/call_out.h"
#include "async/async_runtime.h"
#include "async/console_mode.h"

#include <math.h>

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

/* The 'current_time' is updated at every heart beat. */
time_t current_time = 0;

bool heart_beat_flag = false;

object_t *current_heart_beat;

int64_t eval_cost = 0;

/*
 * Async Runtime - Unified event loop for I/O events and worker completions
 *
 * Uses async_runtime for platform-agnostic event-driven I/O multiplexing.
 * See docs/internals/async-library.md for design.
 */
async_runtime_t *g_runtime = NULL;

/* Console worker globals */
console_worker_context_t *g_console_worker = NULL;
async_queue_t *g_console_queue = NULL;

/*
 * There are global variables that must be zeroed before any execution.
 * In case of runtime errors, boundary exception handling restores control,
 * and these globals still need explicit reset at top-level boundaries.
 * They are normally maintained by the
 * code that use them.
 *
 * This routine must only be called from top level, not from inside
 * stack machine execution (as stack will be cleared).
 */
void init_backend () {
  current_object = 0;
  command_giver = 0;
  current_interactive = 0;
  previous_ob = 0;
  current_prog = 0;
  caller_type = 0;
  reset_interpreter ();		/* Pop down the stack. */
}

/**
 * @brief This function calls the master::connect() to determine whether to accept
 * a new user connection. If accepted, the user object returned by connect() is
 * initialized and returned.
 *
 * @param port The port number on which the new connection was received.
 * @param addr The address of the connecting user as a string.
 * @return The user object if the connection is accepted, NULL otherwise.
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
  ret = APPLY_SLOT_SAFE_MASTER_CALL (APPLY_CONNECT, 1);
  /* master_ob->interactive can be zero if the master object self destructed in the above. */
  if (ret == 0 || ret == (svalue_t *) - 1 || ret->type != T_OBJECT || !master_ob->interactive)
    {
      APPLY_SLOT_FINISH_CALL();
      free_object (master_ob, "mudlib_connect"); /* remove extra reference added when calling connect() */
      debug_message ("connection from %s rejected by master\n", addr);
      return 0;
    }

  ob = ret->u.ob; /* the new user object */
  APPLY_SLOT_FINISH_CALL();

  if (ob->flags & O_HIDDEN)
    num_hidden++;
  ob->interactive = master_ob->interactive;
  ob->interactive->ob = ob;
  ob->flags |= O_ONCE_INTERACTIVE;
  if (is_console_user(ob))
    ob->flags |= O_CONSOLE_USER; /* mark as console user */
  /*
   * assume the existance of write_prompt and process_input in user.c
   * until proven wrong (after trying to call them).
   */
  ob->interactive->iflags |= (HAS_WRITE_PROMPT | HAS_PROCESS_INPUT);

  if (ob == master_ob)
    {
      debug_message ("{}\t***** [%s] connected as single-user.\n", addr);
    }
  else
    {
      master_ob->flags &= ~(O_ONCE_INTERACTIVE|O_CONSOLE_USER);
      master_ob->interactive = 0;
    }
  free_object (master_ob, "mudlib_connect"); /* remove extra reference added when calling connect() */

  add_ref (ob, "mudlib_connect");
  return ob;
}

/**
 * @brief Call the logon() function in the master object after a new user has connected.
 *
 * This allows the MUD to perform any necessary initialization for the new user, such as setting
 * up their environment, sending a welcome message, etc.
 *
 * @param ob The user object that has just connected and for which logon() should be called.
 */
void mudlib_logon (object_t * ob) {
  svalue_t* ret;

  /* current_object no longer set */
  command_giver = ob;
  ret = APPLY_SAFE_CALL (APPLY_LOGON, ob, 0, ORIGIN_DRIVER);
  if (!ret)
    {
      /* TODO: show error in debug log */
      debug_error ("Error occured in logon() of object %s.\n", ob->name);
      return;
    }

  /* function not existing is no longer fatal */
  if (ret == (svalue_t *) - 1)
    {
      debug_warn ("No logon() function in user object %s.\n", ob->name);
      return;
    }
}

/**
 * @brief Initiate connection of the console user in console mode.
 * A console user is a special interactive user that uses the standard
 * input/output of the driver process as the communication channel.
 *
 * When a console user is disconnected, the stdin is not closed, and
 * this function can be called again to reconnect the console user.
 *
 * This is a Neolith extension.
 *
 * @param reconnect If true, indicates that this is a reconnection of an existing console user.
 */
void init_console_user (bool reconnect) {

  object_t* ob;
  if (!master_ob)
    {
      debug_message("No master object loaded, cannot initialize console user.\n");
      return;
    }
  new_interactive (STDIN_FILENO);
  if (!master_ob->interactive)
    {
      debug_message ("Failed to create interactive for console user.\n");
      return;
    }
  master_ob->interactive->connection_type = CONSOLE_USER;
  master_ob->interactive->addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  eval_cost = CONFIG_INT (__MAX_EVAL_COST__);
  ob = mudlib_connect (0, "console"); /* port 0 for console */
  if (!ob)
    {
      if (master_ob->interactive)
        remove_interactive (master_ob, false);
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
    int action = isatty(STDIN_FILENO) ? TCSAFLUSH : TCSANOW;
    tcsetattr (STDIN_FILENO, action, &tio); /* TTY: discard pending input, Pipe: preserve data */
  }
#elif defined(WINSOCK)
  {
    set_console_input_line_mode(1);
    enable_console_output_ansi();
  }
#endif
  if (reconnect)
    {
      /* Any pending input and the ENTER key was discarded after calling tcssetattr() with TCSAFLUSH */
      debug_message("Console user re-connected.\n");
    }
  mudlib_logon (ob);
}

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

typedef struct heart_beat_s {
  object_t *ob;
  short heart_beat_ticks; /* ticks until next heart beat */
  short time_to_heart_beat; /* configured heart beat interval (tick counts) */
} heart_beat_t;

static heart_beat_t *heart_beats = 0;
static int max_heart_beats = 0;
static int heart_beat_index = 0;
static int num_hb_objs = 0;
static int num_hb_to_do = 0;

static int num_hb_calls = 0;	/* starts */
static float perc_hb_probes = 100.0;	/* decaying avge of how many complete */

/**
 * @brief Call all heart_beat() functions in all objects.
 * This function is also responsible for updating the current_time, which makes
 * the time ticking in the MUD.
 * 
 * Also process invocation of LPC reset() and LPC call_out().
 */
void call_heart_beat () {

  object_t *ob;
  heart_beat_flag = false;
  time (&current_time);
  opt_trace (TT_BACKEND|1, "tick: current_time=%u", current_time);
  current_interactive = 0;
  num_hb_to_do = num_hb_objs;

  if ((MAIN_OPTION(timer_flags) & TIMER_FLAG_HEARTBEAT) && (num_hb_to_do > 0))
    {
      heart_beat_t *curr_hb;
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
                  opt_trace (TT_BACKEND|3, "calling heart beat #%d/%d: %s", heart_beat_index + 1, num_hb_to_do, ob->name);
                  /* TODO: catch exceptions */
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

  if (MAIN_OPTION(timer_flags) & TIMER_FLAG_RESET)
    look_for_objects_to_swap (); /* check for LPC object reset() */

  if (MAIN_OPTION(timer_flags) & TIMER_FLAG_CALLOUT)
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
 * Add or remove an object from the heart beat list; does the major check...
 * If an object removes something from the list from within a heart beat,
 * various pointers in call_heart_beat could be stuffed, so we must
 * check current_heart_beat and adjust pointers.
 * @param ob The object to modify.
 * @param to If zero, disable heart beat. If positive, enable/set heart beat ticks
 * @return 1 if successful, 0 on failure (e.g., trying to disable non-enabled heart beat).
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
                heart_beats[index].heart_beat_ticks = (short)to;
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
      hb->time_to_heart_beat = hb->heart_beat_ticks = (short)to;
      ob->flags |= O_HEART_BEAT;
    }

  return 1;
}

int heart_beat_status (outbuffer_t * ob, bool verbose) {
  char buf[20];

  if (verbose)
    {
      outbuf_add (ob, "Heart beat information:\n");
      outbuf_add (ob, "-----------------------\n");
      outbuf_addv (ob, "Number of objects with heart beat: %d, starts: %d\n",
                   num_hb_objs, num_hb_calls);

      /* passing floats to varargs isn't highly portable so let sprintf handle it */
      sprintf (buf, "%.2f", perc_hb_probes);
      outbuf_addv (ob, "Percentage of HB calls completed last time: %s\n", buf);
    }
  return (0);
}				/* heart_beat_status() */

#define NUM_CONSTS 5
static double consts[NUM_CONSTS];

void init_precomputed_tables () {
  int i;

  for (i = 0; i < (int)(sizeof consts / sizeof consts[0]); i++)
    consts[i] = exp (-i / 900.0);
}

static double load_av = 0.0;

void update_load_av () {
  static time_t last_time;
  time_t duration;
  double c;
  static int acc = 0;

  acc++;
  if (current_time == last_time)
    return;
  duration = current_time - last_time;
  if (duration < NUM_CONSTS)
    c = consts[duration];
  else
    c = exp (-duration / 900.0);
  load_av = c * load_av + acc * (1 - c) / duration;
  last_time = current_time;
  acc = 0;
}				/* update_load_av() */

static double compile_av = 0.0;

void update_compile_av (int lines) {
  static time_t last_time;
  time_t duration;
  double c;
  static int acc = 0;

  acc += lines;
  if (current_time == last_time)
    return;
  duration = current_time - last_time;
  if (duration < NUM_CONSTS)
    c = consts[duration];
  else
    c = exp (-duration / 900.0);
  compile_av = c * compile_av + acc * (1 - c) / duration;
  last_time = current_time;
  acc = 0;
}				/* update_compile_av() */

char* query_load_av () {
  static char buff[100];

  sprintf (buff, "%.2f cmds/s, %.2f comp lines/s", load_av, compile_av);
  return (buff);
}				/* query_load_av() */

#ifdef F_HEART_BEATS
array_t* get_heart_beats () {
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
