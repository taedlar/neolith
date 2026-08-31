#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <locale.h>

#include <argparse/argparse.hpp>

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef	HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif	/* HAVE_SYS_WAIT_H */

#define SUPPRESS_COMPILER_INLINES
#include "std.h"
#include "rc/rc.h"
#include "comm.h"
#include "simul_efun.h"
#include "misc/filepath.h"

#ifndef HAVE_REALPATH
extern char* realpath(const char* path, char* resolved_path);
#endif /* !HAVE_REALPATH */

/* prototypes */

static void parse_command_line (int, char **);
static void init_debug_log();
static void print_startup_info();

#ifndef _WIN32
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
#endif /* ! _WIN32 */

/* implementations */

int main (int argc, char **argv) {

  char* locale = setlocale (LC_ALL, PLATFORM_UTF8_LOCALE);

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

  init_stem (0, 0, NULL); /* initialize MAIN_OPTION() defaults */
  parse_command_line (argc, argv); /* parse command line arguments, override MAIN_OPTION() */
  if (!*MAIN_OPTION(config_file) && !*MAIN_OPTION(mud_app))
    {
      fprintf (stderr, "%s: you must specify a configuration file, mudlib archive or master file.\n", argv[0]);
      exit (EXIT_FAILURE);
    }
  init_config (MAIN_OPTION(config_file)); /* initialize CONFIG_STR() / CONFIG_INT() */

  /* Determine launch mode (default: use configured MasterFile) */
  if (*MAIN_OPTION(mud_app))
    {
      char* dot = strrchr (MAIN_OPTION(mud_app), '.');
      if (dot && (strcmp (dot, ".zip") == 0 || strcmp(dot, ".gz") == 0 || strcmp(dot, ".tar") == 0 || strcmp(dot, ".tgz") == 0))
        init_mudlib_archive (MAIN_OPTION(mud_app),
                             MAIN_OPTION(argc) > 0 ? MAIN_OPTION(argv)[0] : ""); /* use the first argument as label if exists */
      else
        init_application (MAIN_OPTION(mud_app), MAIN_OPTION(config_file));
    }

  /************************
   * Initialize debug log *
   ************************/
  init_debug_log();

  /* Print startup banner (and smoke-test debug settings) */
  print_startup_info();
  if (locale)
    LOG_NOTICE ("{}\tusing locale \"%s\"", locale);

  /* Initialize resource pools */
  if (CONFIG_INT (__RESERVED_MEM_SIZE__) > 0)
    {
      reserved_area = (char *) DMALLOC (CONFIG_INT (__RESERVED_MEM_SIZE__), TAG_RESERVED, "main.c: reserved_area");
    } /* malloc.c */
  init_strings (
    CONFIG_INT (__SHARED_STRING_HASH_TABLE_SIZE__),
    CONFIG_INT (__MAX_STRING_LENGTH__)
  );  /* stralloc.c */

  /* Initialize the LPC compiler. */
  init_lpc_compiler (
    CONFIG_INT (__MAX_LOCAL_VARIABLES__),
    CONFIG_STR (__INCLUDE_DIRS__)
  ); /* lib/lpc/compiler.c */
  set_argv_predefine (MAIN_OPTION(argc), MAIN_OPTION(argv)); /* __ARGV__ predefine */

  /* Setup the world simulation machine */
  setup_simulate();

  /* Load and start the mudlib:
   * 1. Load simul_efun object (if any)
   * 2. Load master object
   * 3. Run preload stage (before start listening for connections)
   * 4. Enter backend loop
   */
  if (!stem_startup())
    {
      LOG_FATAL ("{}\t***** error occurs in mudlib startup, shutting down.");
      exit (EXIT_FAILURE);
    }

  if (g_proceeding_shutdown)
    {
      /* It is possible that the mudlib decided to call shutdown() in the preload stage
       * for some reason, e.g. started at wrong time or any fatal error occurred).
       * We should let the mudlib end here gracefully without entering multi-user mode.
       */
      exit (EXIT_SUCCESS);
    }

  /* Run the infinite backend loop */
  stem_run ();

  exit (g_exit_code);
}


static void
parse_command_line (int argc, char *argv[])
{
  argparse::ArgumentParser parser (argv[0], PACKAGE "-" VERSION);
  parser.add_description ("A lightweight LPMud driver (MudOS fork) for easy extend.");
  parser.add_epilog ("MASTER-FILE or MUDLIB-ARCHIVE may be followed by application arguments.");

  parser.add_argument ("-c", "--console-mode")
    .help ("Run the driver in console mode.")
    .flag();
  parser.add_argument ("-d", "--debug")
    .metavar ("debug-level")
    .help ("Specifies the runtime debug level.");
  parser.add_argument ("-D")
    .metavar ("macro[=definition]")
    .append()
    .help ("Predefines a global preprocessor macro for use in the mudlib.");
  parser.add_argument ("-e", "--epilog")
    .metavar ("epilog-level")
    .help ("Specifies the epilog level to be passed to the master object.");
  parser.add_argument ("-f")
    .metavar ("config-file")
    .help ("Specifies the file path of the configuration file.");
  parser.add_argument ("-p", "--pedantic")
    .help ("Enable pedantic clean up.")
    .flag();
  parser.add_argument ("-r", "--timers")
    .metavar ("timers")
    .help ("Specifies timer flags to enable timers (reset, heart_beat, call_out).");
  parser.add_argument ("-t", "--trace")
    .metavar ("trace-flags")
    .help ("Specifies trace flags to enable trace messages in the debug log.");
  parser.add_argument ("application")
    .help ("Master file or mudlib archive, followed by application arguments.")
    .remaining();

  try
    {
      parser.parse_args (argc, argv);
    }
  catch (const std::exception &error)
    {
      fprintf (stderr, "%s\n%s", error.what(), parser.help().str().c_str());
      exit (EXIT_FAILURE);
    }

  if (parser.get<bool> ("--console-mode"))
    MAIN_OPTION(console_mode) = true;
  if (parser.get<bool> ("--pedantic"))
    MAIN_OPTION(pedantic) = true;
  if (const auto value = parser.present<std::string> ("--debug"))
    MAIN_OPTION(debug_level) = atoi (value->c_str());
  if (const auto value = parser.present<std::string> ("--epilog"))
    MAIN_OPTION(epilog_level) = atoi (value->c_str());
  if (const auto value = parser.present<std::string> ("--timers"))
    MAIN_OPTION(timer_flags) = (unsigned int) strtoul (value->c_str(), NULL, 0);
  if (const auto value = parser.present<std::string> ("--trace"))
    MAIN_OPTION(trace_flags) = strtoul (value->c_str(), NULL, 0);

  if (const auto value = parser.present<std::string> ("-f"))
    {
      if (!realpath (value->c_str(), MAIN_OPTION(config_file)))
        {
          debug_perror ("configuration file", value->c_str());
          exit (EXIT_FAILURE);
        }
    }

  if (const auto definitions = parser.present<std::vector<std::string>> ("-D"))
    for (const auto &expression : *definitions)
      {
        lpc_predef_t *def = (lpc_predef_t *) xcalloc (1, sizeof (lpc_predef_t));
        /* argparse owns its parsed strings, while LPC predefines outlive this parser. */
        def->expression = xstrdup (expression.c_str());
        def->next = lpc_predefs;
        lpc_predefs = def;
      }

  if (const auto application = parser.present<std::vector<std::string>> ("application"))
    {
      if (!realpath (application->front().c_str(), MAIN_OPTION(mud_app)))
        {
          perror (application->front().c_str());
          exit (EXIT_FAILURE);
        }
      for (size_t i = 1; i < application->size() && MAIN_OPTION(argc) < MAX_MUD_APP_ARGS; ++i)
        /* __ARGV__ is created after this parser has been destroyed. */
        MAIN_OPTION(argv)[MAIN_OPTION(argc)++] = xstrdup ((*application)[i].c_str());
    }
}

void init_debug_log() {
  int log_severity;

  (void)resolve_mudlib_dir();

  /* allow LogDir be specified as absolute path or relative path to mudlib directory */
  if (CONFIG_STR(__LOG_DIR__))
    {
      char log_dir[PATH_MAX] = "";
      if (filepath_resolve_with_origin (CONFIG_STR(__LOG_DIR__), MAIN_OPTION(mudlib_dir_absolute), log_dir, sizeof(log_dir)))
        {
          SET_CONFIG_STR (__LOG_DIR__, log_dir);

          /* DebugLogFile is always specified relative to the log directory */
          if (CONFIG_STR (__DEBUG_LOG_FILE__))
            {
              char log_file[PATH_MAX];
              if (filepath_join (log_dir, CONFIG_STR (__DEBUG_LOG_FILE__), log_file, sizeof (log_file)))
                {
                  debug_set_log_file (log_file);
                }
            }
        }
    }

  /* log date */
  debug_set_log_with_date (CONFIG_INT (__ENABLE_LOG_DATE__));

  /* log severity: 0 = most verbose, 4 = least verbose */
#ifndef _WIN32
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
  if (MAIN_OPTION(trace_flags))
    {
      log_severity = DEBUG_SEVERITY_TRACE; /* do not filter low severity messages */
    }
  else
    {
      switch (MAIN_OPTION(debug_level))
        {
        case 0:
          log_severity = DEBUG_SEVERITY_WARN;
          break;
        case 1:
          log_severity = DEBUG_SEVERITY_INFO;
          break;
        case 2:
          log_severity = DEBUG_SEVERITY_NOTICE;
          break;
        case 3:
          log_severity = DEBUG_SEVERITY_VERBOSE;
          break;
        default:
          log_severity = DEBUG_SEVERITY_TRACE;
          break;
        }
    }
  debug_set_log_severity (log_severity);
#ifndef _WIN32
#undef max
#endif
}

/**
 * @brief Print startup information to debug log.
 */
static void print_startup_info() {
  LOG_INFO ("{}\t===== %s-%s starting up =====", PACKAGE, VERSION);
#ifdef HAVE_SYS_RESOURCE_H
  struct rlimit rl;
  if (getrlimit (RLIMIT_NOFILE, &rl) == 0)
    {
      LOG_NOTICE ("{}\tmaximum file descriptors: soft=%lu, hard=%lu",
                  (unsigned long)rl.rlim_cur, (unsigned long)rl.rlim_max);
    }
#endif
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
  APPLY_MASTER_CALL (APPLY_CRASH, 3);
  LOG_FATAL ("{}\t***** received SIGUSR1, calling exit(-1)");
  exit (EXIT_FAILURE);
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
  fatal ("***** process terminated");
}

static RETSIGTYPE
sig_int (int sig)
{
  (void)sig; /* unused */
  fatal ("***** process interrupted");
}

static RETSIGTYPE
sig_segv (int sig)
{
  (void)sig; /* unused */
  fatal ("***** segmentation fault");
}

static RETSIGTYPE
sig_bus (int sig)
{
  (void)sig; /* unused */
  fatal ("***** bus error");
}

static RETSIGTYPE
sig_ill (int sig)
{
  (void)sig; /* unused */
  fatal ("***** illegal instruction");
}

static RETSIGTYPE
sig_hup (int sig)
{
  (void)sig; /* unused */
  LOG_NOTICE ("{}\tSIGHUP received, reconfiguration not implemented.\n");
}
#endif /* ! _WIN32 */
