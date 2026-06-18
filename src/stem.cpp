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
#include "misc/filepath.h"
#include "port/timer.h"
#include "rc.h"
#include "simul_efun.h"
#include "simulate.h"

#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

main_options_t *g_main_options = NULL;

bool g_proceeding_shutdown = false;
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
  current_time = time(&boot_time);
  srand((unsigned int)boot_time);

  stem_opts.epilog_level = 0;
  stem_opts.debug_level = debug_level;
  stem_opts.trace_flags = trace_flags;
  stem_opts.console_mode = false;
  stem_opts.pedantic = false;
  stem_opts.timer_flags = TIMER_FLAG_HEARTBEAT | TIMER_FLAG_CALLOUT | TIMER_FLAG_RESET;
  memset(stem_opts.config_file, 0, PATH_MAX);
  memset(stem_opts.mudlib_dir_absolute, 0, PATH_MAX);
  memset(stem_opts.mud_app, 0, PATH_MAX);
  stem_opts.argc = 0;
  if (config_file)
    strncpy (stem_opts.config_file, config_file, PATH_MAX - 1);

  g_main_options = &stem_opts; /* this is required throughout the code*/
  return 0;
}

/**
 * Resolve the absolute path of the mudlib directory and store it in
 * `mudlib_dir_absolute` config option.
 * The resolution is done by resolving the `__MUD_LIB_DIR__` config option relative
 * to the config file location. If `mudlib_dir_absolute` is already set, it will not
 * be resolved again.
 */
extern "C" bool resolve_mudlib_dir (void) {
  if (MAIN_OPTION(mudlib_dir_absolute)[0] != '\0')
    {
      /* mudlib_dir_absolute already set, no need to resolve */
      return true;
    }
  if (CONFIG_STR(__MUD_LIB_DIR__))
    {
      /* Resolve absolute path of mudlib directory relative to the config file location */
      if (filepath_resolve_with_origin (CONFIG_STR (__MUD_LIB_DIR__), MAIN_OPTION(config_file),
                                        MAIN_OPTION(mudlib_dir_absolute), PATH_MAX))
        return true;
    }
  return false;
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

/**
 * @brief Initialize the MUD driver, including loading master and simul_efun
 * objects, and preloading necessary objects.
 * @return true on successful initialization, false if a fatal error occurred
 * during startup.
 */
extern "C"
bool stem_startup(void) {
  error_context_t econ;
  neolith::error_boundary_guard boundary(&econ);

  eval_cost = CONFIG_INT(__MAX_EVAL_COST__);
  try
    {
      if (CONFIG_STR(__SIMUL_EFUN_FILE__))
        {
          LOG_NOTICE ("{}\t----- loading simul efuns -----");
          init_simul_efun (CONFIG_STR(__SIMUL_EFUN_FILE__), NULL);
        }

      LOG_NOTICE ("{}\t----- loading master [%s] -----", CONFIG_STR(__MASTER_FILE__));
      init_master (CONFIG_STR(__MASTER_FILE__), NULL);

      LOG_NOTICE ("{}\t----- epilogue [%d] -----", MAIN_OPTION(epilog_level));
      preload_objects (MAIN_OPTION(epilog_level));

      return true;
    }
  catch (const neolith::driver_runtime_error &)
    {
      boundary.restore();
      return false;
    }
}

/**
 * @brief Despite the name, this routine takes care of several things.
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
extern "C"
void look_for_objects_to_swap(void) {
  static time_t next_time;
  bool clean_up_enabled = CONFIG_INT(__TIME_TO_CLEAN_UP__) > 0;
  int time_to_clean_up = CONFIG_INT(__TIME_TO_CLEAN_UP__);

  if (current_time < next_time)
    return;
  next_time = current_time + 15 * 60;

  while (true)
    {
      error_context_t econ;
      neolith::error_boundary_guard boundary(&econ);

      try
        {
          for (object_t *next_ob, *ob = obj_list; ob; ob = next_ob)
            {
              if (ob->flags & O_DESTRUCTED)
                {
                  ob = obj_list;
                  if (!ob)
                    break;
                }
              next_ob = ob->next_all;
              eval_cost = CONFIG_INT(__MAX_EVAL_COST__);

#ifndef LAZY_RESETS
              if ((ob->flags & O_WILL_RESET) && (ob->next_reset < current_time) && !(ob->flags & O_RESET_STATE))
                reset_object(ob);
#endif

              if (clean_up_enabled && (ob->flags & O_WILL_CLEAN_UP))
                {
                  if (current_time - ob->time_of_ref > time_to_clean_up)
                    clean_up_object(ob);
                }
            }
          break; /* finished without exception */
        }
      catch (const neolith::driver_runtime_error &)
        {
          boundary.restore();
        }
    }
}

#ifdef LAZY_RESETS
extern "C" void try_reset (object_t * ob) {
  if ((ob->next_reset < current_time) && !(ob->flags & O_RESET_STATE))
    {
      /* need to set the flag here to prevent infinite loops in apply_low */
      ob->flags |= O_RESET_STATE;
      reset_object (ob);
    }
}
#endif

/**
 * @brief Validate an LPC path via master object and push the result onto the eval stack.
 *
 * Performs master object permission check (valid_read / valid_write applies),
 * normalises the result (strips leading '/', maps empty to "."), and validates
 * it with legal_path.
 *
 * On success the normalised, legal path is pushed as a malloc string onto sp.
 * The caller is responsible for pop_stack() when done with the path.
 * On failure nothing is pushed and 0 is returned.
 *
 * The push-style return is re-entrant safe: each invocation owns its own stack
 * slot, so nested calls (e.g. valid_write calling a file efun) cannot overwrite
 * each other's result.  On LPC error unwind the eval stack is cleaned up
 * automatically, so no explicit cleanup is needed on error paths.
 *
 * @param path         LPC path to validate (relative or absolute, as seen from LPC code)
 * @param call_object  The LPC object making the call (for master object apply)
 * @param call_fun     The function name calling this (for error messages)
 * @param writeflg     Non-zero if this is for write access, 0 for read
 * @return true on success (path pushed), false on failure (nothing pushed).
 */
bool push_valid_path (const char *path, object_t * call_object, const char *call_fun, int writeflg) {

  static char current_dir[] = ".";
  svalue_t *v;
  const char *ret_path = 0;

  if (call_object == 0 || call_object->flags & O_DESTRUCTED)
    return false;

  copy_and_push_string (path);
  push_object (call_object);
  push_constant_string (call_fun);
  if (writeflg)
    v = APPLY_SLOT_MASTER_CALL (APPLY_VALID_WRITE, 3);
  else
    v = APPLY_SLOT_MASTER_CALL (APPLY_VALID_READ, 3);

  if (v == (svalue_t *) - 1)
    {
      debug_warn ("master object not loaded yet");
      v = 0;
    }
  else if (v)
    {
      if (v->type == T_NUMBER && v->u.number == 0)
        {
          APPLY_SLOT_FINISH_CALL ();
          return false;
        }
      if (v->type == T_STRING)
        {
          ret_path = SVALUE_STRPTR (v);
        }
    }

  if (!ret_path)
    ret_path = path; /* fallback to original path */

  if (ret_path[0] == '/')
    ret_path++; /* strip leading '/' */

  if (ret_path[0] == '\0')
    ret_path = current_dir;

  if (legal_path (ret_path))
    {
      /* Copy before APPLY_SLOT_FINISH_CALL frees the apply slot. */
      malloc_str_t copy = string_copy (ret_path, "push_valid_path");
      APPLY_SLOT_FINISH_CALL ();
      opt_trace (TT_EVAL, "legal path: %s (%s)", path, ret_path);
      push_malloced_string (copy);
      return true;
    }

  APPLY_SLOT_FINISH_CALL ();
  opt_trace (TT_EVAL, "not legal path: %s", path);
  return false;
}

/**
 * @brief Validate an LPC path via master object, resolve to a sandboxed absolute
 *        location, and push the result onto the eval stack.
 *
 * Two-stage process:
 * 1. push_valid_path() — master object permission check.
 * 2. resolve_path_in_mudlib() — anchors to mudlib root and verifies containment.
 *
 * On success the absolute sandboxed path is pushed as a malloc string onto sp.
 * Caller must pop_stack() when done.
 * On failure nothing is pushed and 0 is returned.
 *
 * Re-entrant and exception-unwind safe via the same eval-stack ownership invariant
 * as push_valid_path().
 *
 * @note The resolved path is a host filesystem path, not an LPC path. This function is intended
 *       for internal efun helpers that need to access the host filesystem, and the result is not
 *       suitable for passing back to LPC code or master object applies. The caller contract is that
 *       the result is a "usable path" for host filesystem access, and the caller should not make
 *       any assumptions about the format of the path (e.g. it may be an absolute path, or an
 *       archive-relative path if mudlib is an archive).
 * @note When mudlib is an archive, stage 2 will resolve to an archive-relative path instead
 *       of an absolute filesystem path. The caller contract ("usable path") stays the same.
 *
 * @param path         LPC path to validate (relative or absolute, as seen from LPC code)
 * @param call_object  The LPC object making the call (for master object apply)
 * @param call_fun     The function name calling this (for error messages)
 * @param writeflg     Non-zero if this is for write access, 0 for read
 * @return true on success (path pushed), false on failure (nothing pushed).
 */
bool push_resolved_valid_path (const char *path, object_t * call_object, const char *call_fun, int writeflg) {

  const char *mudlib_dir;
  char *full_path;

  /* Stage 1: Permission check via master object applies. */
  if (!push_valid_path (path, call_object, call_fun, writeflg))
    return false;

  mudlib_dir = MAIN_OPTION (mudlib_dir_absolute);
  if (!mudlib_dir || !*mudlib_dir)
    {
      pop_stack ();
      opt_warn (1, "mudlib_dir_absolute not set; cannot resolve path in %s()", call_fun);
      return false;
    }

  /* Stage 2: Resolve relative path within mudlib root and verify containment. */
  full_path = resolve_path_in_mudlib (SVALUE_STRPTR (sp), mudlib_dir);
  pop_stack (); /* relative path no longer needed */

  if (!full_path)
    {
      opt_warn (1, "rejected unsafe path in %s(): %s", call_fun, path);
      return false;
    }

  push_malloced_string (full_path);
  return true;
}

/**
 * @brief Resolve a relative path to an absolute path within a root directory and
 * verify containment.
 *
 * Takes a relative path (e.g., from push_resolved_valid_path) and root directory,
 * combines them into an absolute (host) path, and verifies the result is within the
 * root to prevent directory traversal attacks.
 *
 * Uses C++17 std::filesystem for robust path normalization and component comparison.
 *
 * @param relative_path The mudlib-local path (e.g., "." or "obj/player")
 * @param root          The absolute root directory
 * @return malloc'd absolute path if valid and within root, nullptr on error.
 *         Returns exactly the root if relative_path is "."
 *         Caller must free the returned string with FREE_MSTR().
 */
extern "C"
malloc_str_t resolve_path_in_mudlib(const char *relative_path, const char *mudlib_root) {
  if (!relative_path || !mudlib_root || mudlib_root[0] == '\0')
    return nullptr;

  // Handle the special case where "." means the mudlib root itself
  if (relative_path[0] == '.' && relative_path[1] == '\0')
    return string_copy (mudlib_root, "resolve_path_in_mudlib");

  // Construct absolute path: mudlib_root / relative_path
  char combined[PATH_MAX];
  if (!filepath_join (mudlib_root, relative_path, combined, sizeof (combined)))
    return nullptr;

  // Verify the combined path is within the mudlib root
  if (!is_path_within_root(combined, mudlib_root))
    return nullptr;

  // Return malloc'd copy of the combined path
  return string_copy (combined, "resolve_path_in_mudlib");
}

/**
 * @brief smart_log() - compiler error logging function.
 *
 * There is an error in a specific file. Ask the master object to log the
 * message somewhere.
 * 
 * @param error_file The file where the error occurred.
 * @param line The line number where the error occurred.
 * @param what The error message to log.
 * @param warning If non-zero, indicates that this is a warning rather than an error.
 */
void smart_log (const char *error_file, int line, const char *what, bool warning) {

  svalue_t *mret = NULL;
  extern int pragmas;
  std::stringstream buff;

  buff << error_file << ":" << line << ": ";
  if (warning)
    buff << "warning: ";
  buff << what;
  if (pragmas & PRAGMA_ERROR_CONTEXT)
    buff << show_error_context();
  buff << "\n";

  if (mud_state() >= MS_PRE_MUDLIB)
    {
      share_and_push_string (error_file);
      copy_and_push_string (buff.str().c_str());
      mret = APPLY_SLOT_SAFE_MASTER_CALL (APPLY_LOG_ERROR, 2);
      APPLY_SLOT_FINISH_CALL();
    }

  if (!mret || mret == (svalue_t *) - 1)
    {
      /* "LPC" \t error_file:line: message */
      LOG_ERROR ("\"LPC\"\t%s", buff.str().c_str());
    }
}				/* smart_log() */

/**
 * @brief Heart beat timer callback.
 * Sets the heart_beat_flag to trigger heart beat processing.
 * Wakes up the async runtime blocking wait to run timer-related tasks.
 */
static void heartbeat_timer_callback(void) {
  async_runtime_t *reactor = get_async_runtime();
  heart_beat_flag = true;
  if (reactor)
    async_runtime_wakeup(reactor);
}

static platform_timer_t heartbeat_timer = {0}; /* cross-platform heart beat timer */

/**
 * @brief Start the heart beat timer if enabled in the configuration.
 *
 * The timer will trigger the heartbeat_timer_callback at the configured interval.
 * The HEARTBEAT_INTERVAL defines the "tick" duration for heart beats, which is the minimum
 * time resolution for all other timers in the MUD.
 *
 * @param timer_flags Flags indicating which timers to start.
 */
extern "C"
void start_timers (unsigned int timer_flags) {
#ifdef HEARTBEAT_INTERVAL
  /* start timer if any of the timer flags are set */
  if (timer_flags & (TIMER_FLAG_HEARTBEAT | TIMER_FLAG_CALLOUT | TIMER_FLAG_RESET))
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
          LOG_NOTICE ("{}\ttimer started (0x%x)\n", timer_flags);
        }
    }
  else
    {
      LOG_NOTICE ("{}\ttimer disabled (0x%x)\n", timer_flags);
    }
#endif /* HEARTBEAT_INTERVAL */
}

/**
 * @brief Run startup preload sequence via safe master apply.
 *
 * Calls master::epilog(eflag) through the slot-wrapper apply contract. If
 * epilog returns an array, that array is retained before slot cleanup and
 * each string element is passed to master::preload().
 * 
 * Any errors in the master object during this process are caught and logged by
 * the driver, and the preload sequence will not be aborted, allowing the MUD to
 * start even if some preloads fail.
 *
 * @param eflag The epilog level to pass to master::epilog.
 */
extern "C"
void preload_objects (int eflag) {
  array_t *prefiles;
  svalue_t *ret;

  /* call master::epilog(eflag) */
  push_number (eflag);
  ret = APPLY_SLOT_SAFE_MASTER_CALL(APPLY_EPILOG, 1);
  if ((ret == 0) || (ret == (svalue_t *) -1) || (ret->type != T_ARRAY))
    {
      APPLY_SLOT_FINISH_CALL();
      LOG_NOTICE ("{}\t----- master epilog(%d) did not return an array, skipping preload.", eflag);
      return;
    }
  prefiles = ret->u.arr;
  prefiles->ref++; /* retain before slot cleanup */
  APPLY_SLOT_FINISH_CALL();

  /* call master::preload() for each file */
  opt_info(1, "Preloading %d objects", prefiles->size);
  for (int i = 0; i < prefiles->size; i++)
    {
      if (prefiles->item[i].type != T_STRING)
        continue;
      eval_cost = CONFIG_INT(__MAX_EVAL_COST__);
      reset_load_object_limits();
      push_svalue (prefiles->item + i);
      (void) APPLY_SAFE_MASTER_CALL (APPLY_PRELOAD, 1);
    }
  free_array (prefiles);
  opt_info (1, "Preloading complete");
}

/** @brief Main driver loop, handling user commands, heart beats,
 *  and periodic tasks.
 */
static void driver_loop(void) {
  struct timeval timeout = {10, 0}; /* 10 seconds timeout for async_runtime_wait */
  error_context_t econ;
  neolith::error_boundary_guard boundary(&econ);

  while (1)
    {
      try
        {
          bool has_pending_commands = false;
          int connected_users = 0;
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
              initiate_slow_shutdown (tmp);
            }

          /* grant command turn to all users; detect pending commands */
          for (int i = 0; i < max_users; i++)
            {
              if (all_users[i])
                {
                  all_users[i]->iflags |= HAS_CMD_TURN;
                  connected_users++;

                  if (cmd_in_buf (all_users[i]))
                    {
                      all_users[i]->iflags |= CMD_IN_BUF;
                      has_pending_commands = true;
                    }
                  else
                    {
                      all_users[i]->iflags &= ~CMD_IN_BUF;
                    }
                }
            }

          /* poll for events from asynchronous runtime */
          nb = do_comm_polling ((heart_beat_flag || has_pending_commands) ? NULL : &timeout);
          if (nb == -1)
            {
              debug_perror ("backend: do_comm_polling", 0);
              fatal ("backend: do_comm_polling failed.\n");
            }

          /* process I/O events (and opportunistic queue drains) */
          process_io();

          /* consume user command turns */
          for (int i = 0; process_user_command() && i < connected_users; i++);

          /* call heart beat if raised. (TODO: use atomic heart_beat_flag) */
          if (heart_beat_flag)
            call_heart_beat();
        }
      catch (const neolith::driver_runtime_error &)
        {
          boundary.restore();
        }
    }
}

/** @brief Handle a crash in the MUD.
 * Calls master::crash() through the slot-wrapper apply contract, passing the
 * crash message and context. If master::crash() is unavailable or fails, logs
 * the crash message through the driver's logging mechanism.
 */
extern "C"
void stem_crash_handler (const char *msg) {
  svalue_t *ret;

  copy_and_push_string (msg);

  if (command_giver)
    push_object(command_giver);
  else
    push_undefined();

  if (current_object)
    push_object(current_object);
  else
    push_undefined();

  ret = APPLY_SLOT_SAFE_MASTER_CALL(APPLY_CRASH, 3);
  if (ret && ret != (svalue_t *)-1)
    {
      LOG_NOTICE ("{}\t----- mudlib crash handler finished, shutdown now.");
    }
  else
    {
      LOG_ERROR ("{}\t----- mudlib crash handler failed, using default handler.");
      LOG_FATAL ("CRASH: %s", msg);
    }
  APPLY_SLOT_FINISH_CALL();
}

/** @brief Run the MUD's main loop.
 */
void stem_run () {

  opt_info (1, "Entering main loop.");

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

  LOG_NOTICE ("{}\t----- entering MUD -----");
  start_timers(MAIN_OPTION(timer_flags));
  if (MAIN_OPTION(console_mode))
    init_console_user(false);

  driver_loop();  /* main driver loop */

#ifdef HEARTBEAT_INTERVAL
  platform_timer_cleanup(&heartbeat_timer);
#endif
  do_shutdown ();

#ifdef WINSOCK
  WSACleanup(); /* for graceful shutdown */
#endif

  opt_info (1, "Leaving main loop.");
}
