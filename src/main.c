#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <errno.h>
#include <stdlib.h>
#include <locale.h>

#ifdef	HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef	HAVE_ARGP_H
#include <argp.h>
#endif /* HAVE_ARGP_H */

#ifdef _WIN32
#include "port/getopt.h"
#endif

#define SUPPRESS_COMPILER_INLINES
#include "std.h"
#include "rc.h"
#include "comm.h"
#include "uids.h"
#include "lpc/object.h"
#include "simul_efun.h"
#include "lpc/program/binaries.h"
#include "lpc/otable.h"
#include "main.h"

#ifdef HAVE_ARGP_H
const char *argp_program_version = PACKAGE "-" VERSION;
const char *argp_program_bug_address = "https://github.com/taedlar/neolith";
#endif /* HAVE_ARGP_H */

/* prototypes */

static void parse_command_line (int, char **);
static void init_debug_log();

static RETSIGTYPE sig_fpe (int sig);
static RETSIGTYPE sig_cld (int sig);

static RETSIGTYPE sig_usr1 (int sig);
static RETSIGTYPE sig_usr2 (int sig);
static RETSIGTYPE sig_term (int sig);
static RETSIGTYPE sig_int (int sig);

static RETSIGTYPE sig_hup (int sig);
static RETSIGTYPE sig_segv (int sig);
static RETSIGTYPE sig_ill (int sig);
static RETSIGTYPE sig_bus (int sig);

/* implementations */

int main (int argc, char **argv) {

  char* locale = NULL;
  error_context_t econ;

#ifndef _WIN32
  /* Setup signal handlers */
  signal (SIGFPE, sig_fpe);
  signal (SIGUSR1, sig_usr1);
  signal (SIGUSR2, sig_usr2);
  signal (SIGTERM, sig_term);
  signal (SIGINT, sig_int);
  signal (SIGHUP, sig_hup);
  signal (SIGBUS, sig_bus);
  signal (SIGSEGV, sig_segv);
  signal (SIGILL, sig_ill);
  signal (SIGCHLD, sig_cld);
#endif

  /* Initialize LPMud driver runtime environment */
  if (NULL != (locale = setlocale (LC_ALL, "")))
    {
#ifdef	ENABLE_NLS
      bindtextdomain (PACKAGE, LOCALEDIR);
      textdomain (PACKAGE);
#endif /* ENABLE_NLS */
    }
  init_stem(0, 0, NULL);
  parse_command_line (argc, argv);
  init_config (MAIN_OPTION(config_file));
  init_debug_log();

  /* Print startup banner (and smoke-test debug settings) */
  if (!debug_message ("{}\t===== %s version %s starting up =====", PACKAGE, VERSION))
    exit (EXIT_FAILURE);
  if (locale)
    debug_message ("{}\tusing locale \"%s\"", locale);

  debug_message ("{}\tusing MudLibDir \"%s\"", CONFIG_STR(__MUD_LIB_DIR__));
  if (-1 == chdir (CONFIG_STR (__MUD_LIB_DIR__)))
    {
      perror (CONFIG_STR (__MUD_LIB_DIR__));
      exit (EXIT_FAILURE);
    }

  /* Initialize resource pools */
  if (CONFIG_INT (__RESERVED_MEM_SIZE__) > 0)
    {
      reserved_area = (char *) DMALLOC (
        CONFIG_INT (__RESERVED_MEM_SIZE__),
        TAG_RESERVED, "main.c: reserved_area");
    } /* malloc.c */
  init_strings (
    CONFIG_INT (__SHARED_STRING_HASH_TABLE_SIZE__),
    CONFIG_INT (__MAX_STRING_LENGTH__)
  );  /* stralloc.c */

  /* Initialize the LPC compiler. */
  init_lpc_compiler(CONFIG_INT (__MAX_LOCAL_VARIABLES__)); /* lib/lpc/compiler.c */
  set_inc_list (CONFIG_STR (__INCLUDE_DIRS__));

  /* Initialize the world simulation machine */
  init_simulate();

  /* Load and start the mudlib:
   * 1. Load simul_efun object (if any)
   * 2. Load master object
   * 3. Run preload stage (before start listening for connections)
   * 4. Enter backend loop
   */
  eval_cost = CONFIG_INT (__MAX_EVAL_COST__);
  save_context (&econ);
  if (setjmp (econ.context))
    {
      /* returned from longjmp() */
      restore_context (&econ);
      pop_context (&econ);
      debug_message ("{}\t***** error occurs in pre-loading stage, shutting down.");
      exit (EXIT_FAILURE);
    }
  else
    {
      debug_message ("{}\t----- loading simul efuns -----");
      init_simul_efun (CONFIG_STR (__SIMUL_EFUN_FILE__)); /* could be NULL */

      debug_message ("{}\t----- loading master -----");
      init_master (CONFIG_STR (__MASTER_FILE__));

      debug_message ("{}\t----- epilogue -----");
      preload_objects (MAIN_OPTION(epilog_level)); /* do epilog() and preload() master applies */
    }
  pop_context (&econ);

  if (g_proceeding_shutdown)
    {
      /* It is possible that the mudlib decided to call shutdown() in the preload stage
       * for some reason, e.g. started at wrong time or any fatal error occurred).
       * We should let the mudlib end here gracefully without entering multi-user mode.
       */
      exit (EXIT_SUCCESS);
    }

  /* Run the infinite backend loop */
  debug_message ("{}\t----- entering MUD -----");
  backend ();

  /* NOTE: We do not do active tear down of the runtime environment when running as
   * a long-lived server process. It is not pratical to require the mudlib to destruct
   * all loaded objects and free all allocated resources in a clean way upon shutdown.
   * All allocated resources will be reclaimed by the operating system upon process
   * termination.
   *
   * However, we do call tear down after running unit tests to ensure that there
   * is no memory leak. The graceful tear down code can be found in various unit-testing
   * code under the tests/ directory.
   */
#if 0
  tear_down_simulate();
  deinit_lpc_compiler();
  deinit_strings();
  deinit_config();  
#endif

  return EXIT_SUCCESS;
}


#ifdef	HAVE_ARGP_H
static error_t
parse_argument (int key, char *arg, struct argp_state *state)
{
  (void)state; /* unused */
  switch (key)
    {
    case 'f':
      if (NULL == realpath (arg, MAIN_OPTION(config_file)))
        {
          perror (arg);
          exit (EXIT_FAILURE);
        }
      break;
    case 'c':
      MAIN_OPTION(console_mode) = 1;
      break;
    case 'D':
      {
        struct lpc_predef_s *def;

        def =  (struct lpc_predef_s *) xcalloc (1, sizeof (struct lpc_predef_s));
        def->flag = arg;
        def->next = lpc_predefs;
        lpc_predefs = def;
        break;
      }
    case 'd':
      MAIN_OPTION(debug_level) = atoi (arg);
      break;
    case 'e':
      MAIN_OPTION(epilog_level) = atoi (arg);
      break;
    case 't':
      MAIN_OPTION(trace_flags) = strtoul (arg, NULL, 0);
      break;
    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}
#endif /* HAVE_ARGP_H */

static void
parse_command_line (int argc, char *argv[])
{
#ifdef	HAVE_ARGP_H
  struct argp_option options[] = {
    {.name = NULL, 'f', "config-file", 0, "Specifies the file path of the configuration file."},
    {.name = NULL, 'D', "macro[=definition]", 0, "Predefines global preprocessor macro for use in mudlib."},
    {.name = "console-mode", 'c', NULL, 0, "Run the driver in console mode."},
    {.name = "debug", 'd', "debug-level", 0, "Specifies the runtime debug level."},
    {.name = "epilog", 'e', "epilog-level", 0, "Specifies the epilog level to be passed to the master object."},
    {.name = "trace", 't', "trace-flags", 0, "Specifies an integer of trace flags to enable trace messages in debug log."},
    {0}
  };
  struct argp parser = {
    .options = options,
    .parser = parse_argument,
    .args_doc = NULL,
    .doc = "\nA lightweight LPMud driver (MudOS fork) for easy extend."
  };

  argp_parse (&parser, argc, argv, 0, 0, 0);
#else /* ! HAVE_ARGP_H */
  int c;

  while ((c = getopt (argc, argv, "f:cd:D:t:e:")) != -1)
    {
      switch (c)
        {
        case 'f':
          if (!realpath (optarg, MAIN_OPTION(config_file)))
            {
              perror (optarg);
              exit (0);
            }
          break;
        case 'c':
          MAIN_OPTION(console_mode) = 1;
          break;
        case 'd':
          MAIN_OPTION(debug_level) = atoi (optarg);
          break;
        case 'e':
          MAIN_OPTION(epilog_level) = atoi (optarg);
          break;
        case 'D':
          {
            struct lpc_predef_s *def;

            def = (struct lpc_predef_s *) xcalloc (1, sizeof (struct lpc_predef_s));
            def->flag = optarg;
            def->next = lpc_predefs;
            lpc_predefs = def;
            break;
          }
        case 't':
          MAIN_OPTION(trace_flags) = strtoul (optarg, NULL, 0);
          break;
        case '?':
        default:
          fatal ("invalid option: %c", c);
        }
    }
#endif /* ! HAVE_ARGP_H */

  if (!*MAIN_OPTION(config_file))
    snprintf (MAIN_OPTION(config_file), PATH_MAX, "/etc/neolith.conf");
}

void init_debug_log()
{
  char path[PATH_MAX];
  if (CONFIG_STR (__DEBUG_LOG_FILE__))
    {
      if (CONFIG_STR (__LOG_DIR__))
        snprintf (path, sizeof(path), "%s/%s", CONFIG_STR (__LOG_DIR__), CONFIG_STR (__DEBUG_LOG_FILE__));
      else
        snprintf (path, sizeof(path), "%s", CONFIG_STR (__DEBUG_LOG_FILE__));
    }
  path[sizeof(path) - 1] = 0;
  debug_set_log_file (path);

  debug_set_log_with_date (CONFIG_INT (__ENABLE_LOG_DATE__));
}

#ifndef _WIN32
static RETSIGTYPE
sig_cld (int sig)
{
  int status;
  (void)sig; /* unused */

  while (wait3 (&status, WNOHANG, NULL) > 0);
}

static RETSIGTYPE
sig_fpe (int sig)
{
  (void)sig; /* unused */
  signal (SIGFPE, sig_fpe);
}

/* send this signal when the machine is about to crash.  The script
   which restarts the MUD should take an exit code of -1 to mean don't
   restart
 */
static RETSIGTYPE
sig_usr1 (int sig)
{
  (void)sig; /* unused */
  push_constant_string ("Host machine shutting down");
  push_undefined ();
  push_undefined ();
  apply_master_ob (APPLY_CRASH, 3);
  debug_message ("{}\t***** received SIGUSR1, calling exit(-1)");
  exit (-1);
}

/* Abort evaluation */
static RETSIGTYPE
sig_usr2 (int sig)
{
  (void)sig; /* unused */
  eval_cost = 1;
}

/*
 * Actually, doing all this stuff from a signal is probably illegal
 * -Beek
 */
static RETSIGTYPE
sig_term (int sig)
{
  (void)sig; /* unused */
  fatal ("process terminated");
}

static RETSIGTYPE
sig_int (int sig)
{
  (void)sig; /* unused */
  fatal ("process interrupted");
}

static RETSIGTYPE
sig_segv (int sig)
{
  (void)sig; /* unused */
  fatal ("segmentation fault");
}

static RETSIGTYPE
sig_bus (int sig)
{
  (void)sig; /* unused */
  fatal ("bus error");
}

static RETSIGTYPE
sig_ill (int sig)
{
  (void)sig; /* unused */
  fatal ("illegal instruction");
}

static RETSIGTYPE
sig_hup (int sig)
{
  (void)sig; /* unused */
  debug_message ("SIGHUP received, reconfiguration not implemented.\n");

#if 0
  g_proceeding_shutdown = 1;
#endif
}
#endif /* ! _WIN32 */
