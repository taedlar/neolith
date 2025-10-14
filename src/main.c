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

#define SUPPRESS_COMPILER_INLINES
#include "std.h"
#include "lpc/types.h"
#include "rc.h"
#include "stralloc.h"
#include "lpc/object.h"
#include "simulate.h"
#include "applies.h"
#include "backend.h"
#include "simul_efun.h"
#include "binaries.h"
#include "lpc/otable.h"
#include "comm.h"
#include "main.h"
#include "port/wrapper.h"

const char *argp_program_version = PACKAGE "-" VERSION;
const char *argp_program_bug_address = "https://github.com/taedlar/neolith";

server_options_t* g_svropts = NULL;

port_def_t external_port[5];

int g_proceeding_shutdown = 0;

int t_flag = 0;			/* Disable heart beat and reset */
int comp_flag = 0;		/* Trace compilations */
int boot_time;
char *reserved_area;		/* reserved for MALLOC() */

svalue_t const0, const1, const0u;

/* -1 indicates that we have never had a master object.  This is so the
 * simul_efun object can load before the master. */
object_t *master_ob = 0;

/* prototypes */

static void parse_command_line (int, char **);

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

int
main (int argc, char **argv)
{
  static server_options_t svropts;
  char* locale = NULL;
  time_t now;
  error_context_t econ;

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

  if (NULL != (locale = setlocale (LC_ALL, ""))) {
#ifdef	ENABLE_NLS
    bindtextdomain (PACKAGE, LOCALEDIR);
    textdomain (PACKAGE);
#endif /* ENABLE_NLS */
  }

  memset (&svropts, 0, sizeof (svropts));
  g_svropts = &svropts;
  parse_command_line (argc, argv);

  tzset ();
  boot_time = current_time = time (&now);

  srand (boot_time);

  const0.type = T_NUMBER;
  const0.u.number = 0;
  const1.type = T_NUMBER;
  const1.u.number = 1;

  /* const0u used by undefinedp() */
  const0u.type = T_NUMBER;
  const0u.subtype = T_UNDEFINED;
  const0u.u.number = 0;
  fake_prog.program_size = 0;

  init_config (SERVER_OPTION(config_file));

  // print a startup banner in the log file (to test if the debug log is created successfully)
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

  init_strings ();		/* stralloc.c */
  init_objects ();		/* object.c */
  init_otable (__OBJECT_HASH_TABLE_SIZE__);		/* otable.c */
  init_identifiers ();		/* lex.c */
  init_locals ();		/* compiler.c */

  set_inc_list (CONFIG_STR (__INCLUDE_DIRS__));
  if (CONFIG_INT (__RESERVED_MEM_SIZE__) > 0)
    reserved_area = (char *) DMALLOC (CONFIG_INT (__RESERVED_MEM_SIZE__),
                                      TAG_RESERVED, "main.c: reserved_area");

  init_precomputed_tables ();
  init_num_args ();
  reset_machine ();

  init_binaries ();
  add_predefines ();

  eval_cost = CONFIG_INT (__MAX_EVAL_COST__);
  save_context (&econ);
  if (setjmp (econ.context))
    {
      debug_message (_("{}\t***** error occurs in pre-loading stage, shutting down."));
      exit (EXIT_FAILURE);
    }
  else
    {
      debug_message ("{}\t----- loading simul efuns -----");
      init_simul_efun (CONFIG_STR (__SIMUL_EFUN_FILE__));

      debug_message ("{}\t----- loading master -----");
      init_master (CONFIG_STR (__MASTER_FILE__));

      debug_message ("{}\t----- epilogue -----");
      preload_objects (0);
    }
  pop_context (&econ);

  if (g_proceeding_shutdown)
    {
      /* the mudlib has decided to shutdown in pre-loading stage, exit now. */
      exit (EXIT_SUCCESS);
    }

  debug_message (_("{}\t----- entering MUD -----"));
  backend ();

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
      if (NULL == realpath (arg, SERVER_OPTION(config_file)))
        {
          perror (arg);
          exit (EXIT_FAILURE);
        }
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
      SERVER_OPTION(debug_level) = atoi (arg);
      break;
    case 't':
      SERVER_OPTION(trace_flags) = strtoul (arg, NULL, 0);
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
    {NULL, 'f', _("config-file"), 0,
     _("Specifies the file path of the configuration file.")},
    {NULL, 'D', _("macro[=definition]"), 0,
     _("Predefines global preprocessor macro for use in mudlib.")},
    {"debug", 'd', _("debug-level"), 0,
     _("Specifies the runtime debug level.")},
    {"trace", 't', _("trace-flags"), 0,
     _("Specifies an integer of trace flags to enable trace messages in debug log.")},
    {0}
  };
  struct argp parser = {
    options,
    parse_argument,
    NULL,
    _("\nA lightweight LPMud driver (MudOS fork) for easy extend.")
  };

  argp_parse (&parser, argc, argv, 0, 0, 0);
#else /* ! HAVE_ARGP_H */
  int c;

  while ((c = getopt (argc, argv, "f:d:D:t:")) != -1)
    {
      switch (c)
        {
        case 'f':
          if (!realpath (optarg, SERVER_OPTION(config_file)))
            {
              perror (optarg);
              exit (0);
            }
          break;
        case 'd':
          SERVER_OPTION(debug_level) = atoi (optarg);
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
          SERVER_OPTION(trace_flags) = strtoul (optarg, NULL, 0);
          break;
        case '?':
        default:
          fatal (_("invalid option: %c"), c);
        }
    }
#endif /* ! HAVE_ARGP_H */

  if (!*SERVER_OPTION(config_file))
    snprintf (SERVER_OPTION(config_file), PATH_MAX, "/etc/neolith.conf");
}

int slow_shut_down_to_do = 0;

char *
xalloc (int size)
{
  char *p;
  static int going_to_exit = 0;

  if (going_to_exit)
    exit (3);
  p = (char *) DMALLOC (size, TAG_MISC, "main.c: xalloc");
  if (p == 0)
    {
      if (reserved_area)
        {
          FREE (reserved_area);
          /* after freeing reserved area, we are supposed to be able to write log messages */
          debug_message ("{}\t***** temporarily out of MEMORY. Freeing reserve.");
          reserved_area = 0;
          slow_shut_down_to_do = 6;
          return xalloc (size);	/* Try again */
        }
      going_to_exit = 1;
      fatal ("Totally out of MEMORY.\n");
    }
  return p;
}

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

/* send this signal when the machine is about to reboot.  The script
   which restarts the MUD should take an exit code of 1 to mean don't
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
