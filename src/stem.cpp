#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "addr_resolver.h"
#include "apply.h"
#include "backend.h"
#include "comm.h"
#include "command.h"
#include "error_context.h"
#include "lpc/array.h"
#include "lpc/object.h"
#include "lpc/program.h"
#include "lpc/include/origin.h"
#include "port/timer.h"
#include "rc.h"
#include "simul_efun.h"
#include "simulate.h"

main_options_t *g_main_options = NULL;

int g_proceeding_shutdown = 0;
int g_exit_code = EXIT_SUCCESS;

int comp_flag = 0;    /* Trace compilations */
time_t boot_time = 0L;

int slow_shutdown_to_do = 0;

extern "C"
int init_stem(int debug_level, unsigned long trace_flags, const char *config_file) {
  static main_options_t stem_opts;

#ifdef _WIN32
  _tzset();
#else
  tzset();
#endif
  current_time = boot_time = time(NULL);
  srand((unsigned int)boot_time);

  stem_opts.epilog_level = 0;
  stem_opts.debug_level = debug_level;
  stem_opts.trace_flags = trace_flags;
  stem_opts.console_mode = 0;
  stem_opts.pedantic = 0;
  stem_opts.timer_flags = TIMER_FLAG_HEARTBEAT | TIMER_FLAG_CALLOUT | TIMER_FLAG_RESET;
  memset(stem_opts.config_file, 0, PATH_MAX);
  if (config_file)
    strncpy(stem_opts.config_file, config_file, PATH_MAX - 1);

  g_main_options = &stem_opts; /* this is required throughout the code*/
  return 0;
}

static int normalize_runtime_setting(int value) {
  return value >= 0 ? value : 0;
}

extern "C"
void stem_get_addr_resolver_config(addr_resolver_config_t *config) {
  if (!config)
    return;

  addr_resolver_config_init_defaults(config);
  config->forward_cache_ttl = normalize_runtime_setting(CONFIG_INT(__RESOLVER_FORWARD_CACHE_TTL__));
  config->reverse_cache_ttl = normalize_runtime_setting(CONFIG_INT(__RESOLVER_REVERSE_CACHE_TTL__));
  config->negative_cache_ttl = normalize_runtime_setting(CONFIG_INT(__RESOLVER_NEGATIVE_CACHE_TTL__));
  config->stale_refresh_window = normalize_runtime_setting(CONFIG_INT(__RESOLVER_STALE_REFRESH_WINDOW__));
  config->forward_quota = normalize_runtime_setting(CONFIG_INT(__RESOLVER_FORWARD_QUOTA__));
  config->reverse_quota = normalize_runtime_setting(CONFIG_INT(__RESOLVER_REVERSE_QUOTA__));
  config->refresh_quota = normalize_runtime_setting(CONFIG_INT(__RESOLVER_REFRESH_QUOTA__));
}

extern "C"
int stem_startup(void) {
  error_context_t econ;

  eval_cost = CONFIG_INT(__MAX_EVAL_COST__);

  try
    {
      neolith::error_boundary_guard boundary(&econ);

      try
        {
          current_time = time(NULL);

          debug_message("{}\t----- loading simul efuns -----");
          init_simul_efun(CONFIG_STR(__SIMUL_EFUN_FILE__));

          debug_message("{}\t----- loading master -----");
          init_master(CONFIG_STR(__MASTER_FILE__), NULL);

          debug_message("{}\t----- epilogue -----");
          preload_objects(MAIN_OPTION(epilog_level));

          return 1;
        }
      catch (const neolith::driver_runtime_error &)
        {
          boundary.restore();
          return 0;
        }
    }
  catch (const neolith::driver_runtime_error &)
    {
      return 0;
    }
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
extern "C"
void look_for_objects_to_swap(void) {
  static time_t next_time;

  if (current_time < next_time)
    return;
  next_time = current_time + 15 * 60;

  while (1)
    {
      object_t *next_ob;
      object_t *ob;
      error_context_t econ;
      int had_error = 0;
      neolith::error_boundary_guard boundary(&econ);

      try
        {
          for (ob = obj_list; ob; ob = next_ob)
            {
              time_t ref_time;

              if (ob->flags & O_DESTRUCTED)
                {
                  ob = obj_list;
                  if (!ob)
                    break;
                }

              next_ob = ob->next_all;
              ref_time = ob->time_of_ref;
              eval_cost = CONFIG_INT(__MAX_EVAL_COST__);

#ifndef LAZY_RESETS
              if ((ob->flags & O_WILL_RESET) && (ob->next_reset < current_time)
                  && !(ob->flags & O_RESET_STATE))
                {
                  reset_object(ob);
                }
#endif

              if (CONFIG_INT(__TIME_TO_CLEAN_UP__) > 0)
                {
                  if (current_time - ref_time > CONFIG_INT(__TIME_TO_CLEAN_UP__)
                      && (ob->flags & O_WILL_CLEAN_UP))
                    {
                      clean_up_object(ob);
                    }
                }
            }
        }
      catch (const neolith::driver_runtime_error &)
        {
          boundary.restore();
          had_error = 1;
        }

      if (!had_error)
        {
          break;
        }
    }
}

/**
 * @brief Heart beat timer callback.
 * Sets the heart_beat_flag to trigger heart beat processing.
 * Wakes up the async runtime blocking wait to run timer-related tasks.
 */
static void heartbeat_timer_callback(void) {
  async_runtime_t *reactor = get_async_runtime();
  heart_beat_flag = 1;
  if (reactor)
    async_runtime_wakeup(reactor);
}

static platform_timer_t heartbeat_timer = {0}; /* cross-platform heart beat timer */

extern "C"
void start_timers(void) {
#ifdef HEARTBEAT_INTERVAL
  /* start timer if any of the timer flags are set */
  if (MAIN_OPTION(timer_flags) & (TIMER_FLAG_HEARTBEAT | TIMER_FLAG_CALLOUT | TIMER_FLAG_RESET))
    {
      timer_error_t timer_err;
      timer_err = platform_timer_init(&heartbeat_timer);
      if (timer_err != TIMER_OK)
        {
          opt_warn (0, "Timer initialization failed: %s. heart_beat(), call_out() and reset() disabled.",
                    timer_error_string(timer_err));
        }
      else
        {
          timer_err = platform_timer_start(&heartbeat_timer, HEARTBEAT_INTERVAL, heartbeat_timer_callback);
          if (timer_err != TIMER_OK)
            {
              opt_warn (0, "Timer start failed: %s. heart_beat(), call_out() and reset() disabled.",
                        timer_error_string(timer_err));
            }
          debug_message ("timer started (timer flags = %d)\n", MAIN_OPTION(timer_flags));
        }
    }
  else
    {
      debug_message ("timer not used (timer flags = %d)\n", MAIN_OPTION(timer_flags));
    }
  /* do initial timer tick (initialize current_time and allow LPC code to access time).
   * This is always done even if no timer is started, so that current_time is valid.
   */
  call_heart_beat ();
#endif /* HEARTBEAT_INTERVAL */
}

/**
 * @brief Run startup preload sequence via the guarded C++ path.
 *
 * Calls master::epilog(eflag) through the slot-wrapper apply contract. If
 * epilog returns an array, that array is retained before slot cleanup and
 * each string element is passed to master::preload().
 *
 * Errors during epilog or an individual preload file are contained by the
 * guarded path so startup can continue where possible.
 */
extern "C"
void preload_objects(int eflag) {
  array_t *prefiles;
  svalue_t *ret;
  int ix;
  error_context_t econ;

  try
    {
      neolith::error_boundary_guard boundary(&econ);

      try
        {
          push_number(eflag);
          ret = APPLY_SLOT_SAFE_MASTER_CALL(APPLY_EPILOG, 1);
        }
      catch (const neolith::driver_runtime_error &)
        {
          boundary.restore();
          return;
        }

      if ((ret == 0) || (ret == (svalue_t *) -1) || (ret->type != T_ARRAY))
        {
          APPLY_SLOT_FINISH_CALL();
          return;
        }
      prefiles = ret->u.arr;
      if ((prefiles == 0) || (prefiles->size < 1))
        {
          APPLY_SLOT_FINISH_CALL();
          return;
        }

      // Retain the array before finishing the slot
      prefiles->ref++;
      APPLY_SLOT_FINISH_CALL();

      opt_info(1, "Preloading %d objects", prefiles->size);
      ix = 0;

      while (ix < prefiles->size)
        {
          int had_error = 0;

          try
            {
              error_context_t preload_econ;
              neolith::error_boundary_guard preload_boundary(&preload_econ);

              try
                {
                  for (; ix < prefiles->size; ix++)
                    {
                      if (prefiles->item[ix].type != T_STRING)
                        continue;
                      eval_cost = CONFIG_INT(__MAX_EVAL_COST__);
                      push_svalue(prefiles->item + ix);
                      (void) APPLY_MASTER_CALL (APPLY_PRELOAD, 1);
                    }
                }
              catch (const neolith::driver_runtime_error &)
                {
                  preload_boundary.restore();
                  had_error = 1;
                }
            }
          catch (const neolith::driver_runtime_error &)
            {
              break;
            }

          if (had_error)
            {
              opt_warn(1, "Error preloading file %d/%d, continuing.", ix + 1, prefiles->size);
              ix++;
              continue;
            }
          break;
        }

      free_array(prefiles);

      opt_info(1, "Preloading complete");
    }
  catch (const neolith::driver_runtime_error &)
    {
      return;
    }
}

/** @brief Main driver loop, handling user commands, heart beats,
 *  and periodic tasks.
 */
void driver_loop(void) {
  error_context_t econ;
  neolith::error_boundary_guard boundary(&econ);

  if (MAIN_OPTION(console_mode))
    init_console_user(0);

  while (1)
    {
      try
        {
          while (1)
            {
              struct timeval timeout;
              int has_pending_commands = 0;
              int connected_users = 0;
              int i;
              int nb;

              current_interactive = 0;
              eval_cost = CONFIG_INT(__MAX_EVAL_COST__);

              if (g_proceeding_shutdown)
                {
                  boundary.restore();
                  return;
                }

              remove_destructed_objects();

              if (slow_shutdown_to_do)
                {
                  int tmp = slow_shutdown_to_do;
                  slow_shutdown_to_do = 0;
                  do_slow_shutdown(tmp);
                }

              for (i = 0; i < max_users; i++)
                {
                  if (all_users[i])
                    {
                      all_users[i]->iflags |= HAS_CMD_TURN;
                      connected_users++;

                      if (!has_pending_commands && (all_users[i]->iflags & CMD_IN_BUF))
                        {
                          has_pending_commands = 1;
                        }
                    }
                }

              if (heart_beat_flag || has_pending_commands)
                {
                  timeout.tv_sec = 0;
                  timeout.tv_usec = 0;
                }
              else
                {
                  timeout.tv_sec = 60;
                  timeout.tv_usec = 0;
                }

              nb = do_comm_polling(&timeout);
              if (nb == -1)
                {
                  debug_perror("backend: do_comm_polling", 0);
                  fatal("backend: do_comm_polling failed.\n");
                }

              if (nb > 0)
                process_io();

              for (i = 0; process_user_command() && i < connected_users; i++);

              if (heart_beat_flag)
                call_heart_beat();
            }
        }
      catch (const neolith::driver_runtime_error &)
        {
          boundary.restore();
          if (MAIN_OPTION(console_mode))
            init_console_user(0);
        }
    }
}

extern "C"
void stem_crash_handler(const char *msg) {
  error_context_t econ;

  try
    {
      neolith::error_boundary_guard boundary(&econ);

      try
        {
          svalue_t *ret;

          copy_and_push_string(msg);

          if (command_giver)
            push_object(command_giver);
          else
            push_undefined();

          if (current_object)
            push_object(current_object);
          else
            push_undefined();

          ret = APPLY_SLOT_MASTER_CALL(APPLY_CRASH, 3);
          if (ret && ret != (svalue_t *)-1)
            {
              debug_message("{}\t----- mudlib crash handler finished, shutdown now.");
            }
          APPLY_SLOT_FINISH_CALL();
        }
      catch (const neolith::driver_runtime_error &)
        {
          boundary.restore();
          debug_message("{}\t***** error in master::%s(), shutdown now.", APPLY_CRASH);
        }
    }
  catch (const neolith::driver_runtime_error &)
    {
      debug_message("{}\t***** error in master::%s(), shutdown now.", APPLY_CRASH);
    }
}

/** @brief Run the MUD's main loop.
 */
void stem_run () {

  opt_info (1, "Entering backend loop.");

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
  init_user_conn();		/* initialize user connection socket */
  init_backend();
  start_timers();

  driver_loop();

  platform_timer_cleanup(&heartbeat_timer);
}
