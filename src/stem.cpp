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
int run_mudlib_startup_guarded(void) {
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

extern "C" void backend_call_heart_beat(void);

extern "C"
void look_for_objects_to_swap_guarded(void) {
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

      if (!save_context(&econ))
        {
          return;
        }


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
                      int save_reset_state = ob->flags & O_RESET_STATE;
                      svalue_t *svp;

                      push_number((ob->flags & O_CLONE) ? 0 : ob->prog->ref);
                      svp = apply(APPLY_CLEAN_UP, ob, 1, ORIGIN_DRIVER);
                      if (ob->flags & O_DESTRUCTED)
                        continue;
                      if (!svp || (svp->type == T_NUMBER && svp->u.number == 0))
                        ob->flags &= ~O_WILL_CLEAN_UP;
                      ob->flags |= save_reset_state;
                    }
                }
            }
        }
      catch (const neolith::driver_runtime_error &)
        {
          restore_context(&econ);
          had_error = 1;
        }

      pop_context(&econ);

      if (!had_error)
        {
          break;
        }
    }
}

extern "C"
void preload_objects_guarded(int eflag) {
  array_t *prefiles;
  svalue_t *ret;
  int ix;
  error_context_t econ;

  if (!save_context(&econ))
    return;


  try
    {
      push_number(eflag);
      ret = apply_master_ob(APPLY_EPILOG, 1);
    }
  catch (const neolith::driver_runtime_error &)
    {
      restore_context(&econ);
      pop_context(&econ);
      return;
    }

  pop_context(&econ);

  if ((ret == 0) || (ret == (svalue_t *) -1) || (ret->type != T_ARRAY))
    return;
  prefiles = ret->u.arr;
  if ((prefiles == 0) || (prefiles->size < 1))
    return;

  opt_info(1, "Preloading %d objects", prefiles->size);

  prefiles->ref++;
  ix = 0;

  while (ix < prefiles->size)
    {
      error_context_t preload_econ;
      int had_error = 0;

      if (!save_context(&preload_econ))
        {
          break;
        }


      try
        {
          for (; ix < prefiles->size; ix++)
            {
              if (prefiles->item[ix].type != T_STRING)
                continue;
              eval_cost = CONFIG_INT(__MAX_EVAL_COST__);
              push_svalue(prefiles->item + ix);
              (void) apply_master_ob(APPLY_PRELOAD, 1);
            }
        }
      catch (const neolith::driver_runtime_error &)
        {
          restore_context(&preload_econ);
          had_error = 1;
        }

      pop_context(&preload_econ);

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

extern "C"
void backend_run_loop_guarded(void) {
  error_context_t econ;

  if (!save_context(&econ))
    {
      fatal("backend: failed to establish recovery context.\n");
    }


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
                  pop_context(&econ);
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
                backend_call_heart_beat();
            }
        }
      catch (const neolith::driver_runtime_error &)
        {
          restore_context(&econ);
          if (MAIN_OPTION(console_mode))
            init_console_user(0);
        }
    }
}

extern "C"
void invoke_master_crash_handler_guarded(const char *msg) {
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

          ret = apply_master_ob(APPLY_CRASH, 3);
          if (ret && ret != (svalue_t *)-1)
            {
              debug_message("{}\t----- mudlib crash handler finished, shutdown now.");
            }
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