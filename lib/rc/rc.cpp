/*
    ORIGINAL AUTHOR
        erikkay@mit.edu

    MODIFIED BY
        [Jul  4, 1994] by robo
        [Mar 26, 1995] by Beek
        [Jun 13, 2001] by Annihilator <annihilator@muds.net>
*/

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _WIN32
#include <io.h>
#define open        _open
#define fdopen      _fdopen
#define close       _close
#define strcasecmp  _stricmp
#define strncasecmp _strnicmp
#define PATH_MAX    MAX_PATH
#endif

#include "port/wrapper.h"
#include "port/debug.h"
#include "rc.h"

#include <filesystem>

int g_trace_flag = 0;

char *config_str[NUM_CONFIG_STRS]; /* NULL or malloc'd strings */
int config_int[NUM_CONFIG_INTS];

port_def_t external_port[5];

/* static declarations */

static int fatal_config_error = 0;

static char* read_config_malloc (const char *filename);
static char* scan_config (char *config, const char *name, bool required, char *def);
static int scan_config_int (char *config, const char *name, bool required, int def);
static int scan_config_bool (char *config, const char *name, bool required, bool def);

/* implementations */

static char* read_config_malloc (const char *filename) {
  struct stat st;
#ifdef	HAVE_GETLINE
  char *line = NULL;
  size_t sz_line = 0;
#else /* ! HAVE_GETLINE */
  char line[1024];
#endif /* ! HAVE_GETLINE */
  char *conf, *p, *s;
  int fd;
  FILE *f;

  if (-1 == (fd = open (filename, O_RDONLY)))
    return NULL;

  if (-1 == fstat (fd, &st) || NULL == (f = fdopen (fd, "r"))) {
    close (fd);
    return NULL;
  }

  conf = (char *) calloc (st.st_size + 1, sizeof (char));
  if (!conf) {
    fclose (f);
    return NULL;
  }

#ifdef	HAVE_GETLINE
  for (p = conf; -1 != getline (&line, &sz_line, f);)
#else /* ! HAVE_GETLINE */
  for (p = conf; NULL != fgets (line, sizeof (line) - 1, f);)
#endif /* ! HAVE_GETLINE */
  {
    for (s = line; isspace (*s); s++);	/* skip leading blanks */
    if (*s == '#' || *s == '\n')
      continue;			/* skip comments and empty lines */

    p = stpcpy (p, s);
  }
  fclose (f);
#ifdef HAVE_GETLINE
  if (line)
    free (line);
#endif
  *p++ = '\0';

  return (char *) realloc (conf, p - conf);
}


/**
 *  @brief Scan and extract a configuration value from the config data (modified in place).
 *  @param config The configuration data (will be modified).
 *  If \p config is NULL or empty, this function will return the default value (ignoring \p required).
 *  @param name The name of the configuration item to search for.
 *  @param required Flag indicating if the configuration item is required in the configuration.
 *  @param def Default value to return if the item is not found or if \p config is NULL or empty.
 *  @return The extracted configuration value or the default value (allocated, if not NULL).
 */
static char* scan_config (char *config, const char *name, bool required, char *def) {
  size_t sz_name;
  char *term, *p;

  if (!config)
    return def ? xstrdup(def) : NULL;
  sz_name = strlen (name);

  for (term = config; term; term = strchr (term, '\n'))
    {
      if (*term == '\n')
        term++;
      if (strncasecmp (name, term, sz_name) || !isspace (term[sz_name]))
        continue;

      *term = '#';

      term += sz_name;
      while (isspace (*term))
        term++;

      if ((p = strchr (term, '\n'))) {
        while (isspace (*(p-1)))
          p--;
        *p = '\0';
      }
      term = xstrdup (term);
      if (p)
        *p = '\n';
      return term;
    }

  if (required)
    {
      debug_error ("%s is missing", name);
      fatal_config_error++;
      return NULL;
    }

  return def ? xstrdup (def) : NULL;
}

static int scan_config_int (char *config, const char *name, bool required, int def) {
  char *term;
  int n;

  if (NULL == (term = scan_config (config, name, required, NULL)))
    return def;

  n = atoi (term);
  free (term);

  return n;
}

static int scan_config_bool (char *config, const char *name, bool required, bool def) {
  char *p;
  int result = def ? 1 : 0;

  while ((p = scan_config (config, name, required, NULL)))
    {
      if (!strcasecmp (p, "yes") || !strcasecmp (p, "true") || !strcasecmp (p, "on") || !strcmp (p, "1"))
        result = 1;
      else if (!strcasecmp (p, "no") || !strcasecmp (p, "false") || !strcasecmp (p, "off") || !strcmp (p, "0"))
        result = 0;
      else
        debug_message ("warnning: %s must be 'yes/no' or 'true/false' [got: %s]\n", name, p);
      free (p);
      required = false; /* only consider the first occurrence of the config item */
    }

  return result;
}

extern "C"
void init_config (const char *config_file) {
  int i;
  char *config, *p;

  for (i = 0; i < NUM_CONFIG_INTS; i++)
    config_int[i] = 0;
  for (i = 0; i < NUM_CONFIG_STRS; i++)
    config_str[i] = 0;

  if (config_file && *config_file)
    {
      if (NULL == (config = read_config_malloc (config_file)))
        {
          perror (config_file);
          exit (EXIT_FAILURE);
        }
    }
  else
    config = NULL; /* no config file specified */

  CONFIG_STR (__LOG_DIR__) = scan_config (config, "LogDir", false, NULL);
  if (CONFIG_STR (__LOG_DIR__))
    {
      // we only assign DebugLogFile if LogDir is specified because the debug log file
      // is independent from the mudlib directory. If LogDir is not specified, we'll
      // ignore the DebugLogFile setting and write debug logs directly to standard error.
      //
      // This guarantees that debug logs are always written to the console when LogDir
      // is not set, which is useful for a read-only mudlib environment.
      CONFIG_STR (__DEBUG_LOG_FILE__) = scan_config (config, "DebugLogFile", false, NULL);
    }
  CONFIG_INT (__ENABLE_LOG_DATE__) = scan_config_bool (config, "LogWithDate", false, false);

  CONFIG_STR (__MUD_LIB_DIR__) = scan_config (config, "MudlibDir", true, NULL); // required
  CONFIG_STR (__MUD_NAME__) = scan_config (config, "MudName", false, NULL);
  CONFIG_STR (__ADDR_SERVER_IP__) = scan_config (config, "AddrServerIP", false, NULL);

  CONFIG_STR (__GLOBAL_INCLUDE_FILE__) = scan_config (config, "GlobalInclude", false, NULL);
  CONFIG_STR (__BIN_DIR__) = NULL;
  CONFIG_STR (__INCLUDE_DIRS__) = scan_config (config, "IncludeDir", false, NULL);
  CONFIG_STR (__SAVE_BINARIES_DIR__) = scan_config (config, "SaveBinaryDir", false, NULL);
  CONFIG_STR (__MASTER_FILE__) = scan_config (config, "MasterFile", true, NULL); // required
  CONFIG_STR (__SIMUL_EFUN_FILE__) = scan_config (config, "SimulEfunFile", false, NULL);
  CONFIG_STR (__DEFAULT_ERROR_MESSAGE__) = scan_config (config, "DefaultErrorMsg", false, NULL);
  CONFIG_STR (__DEFAULT_FAIL_MESSAGE__) = scan_config (config, "DefaultFailMsg", false, NULL);

  for (i = 0; i < 5 && (p = scan_config (config, "Port", false, NULL)); i++)
    {
      char *typ;

      external_port[i].port = strtoul (p, &typ, 0);
      if (!strcasecmp (typ, ":telnet"))
        external_port[i].kind = PORT_TELNET;
      else if (!strcasecmp (typ, ":binary"))
        external_port[i].kind = PORT_BINARY;
      else if (!strcasecmp (typ, ":ascii"))
        external_port[i].kind = PORT_ASCII;
      else if (*typ)
        {
          debug_message ("*Protocol of port %d is invalid, assuming TELNET\n", external_port[i].port);
          external_port[i].kind = PORT_TELNET;
        }
      free (p);
    }
  CONFIG_INT (__MUD_PORT__) = (i > 0) ? external_port[0].port : 0; /* external port is optional since we have console-mode */

  CONFIG_INT (__ADDR_SERVER_PORT__) = scan_config_int (config, "AddrServerPort", false, 0);
  CONFIG_INT (__TIME_TO_CLEAN_UP__) = scan_config_int (config, "CleanupDuration", false, 600);
  CONFIG_INT (__TIME_TO_RESET__) = scan_config_int (config, "ResetDuration", false, 1800);
  CONFIG_INT (__INHERIT_CHAIN_SIZE__) = scan_config_int (config, "MaxInheritDepth", false, 30);
  CONFIG_INT (__MAX_EVAL_COST__) = scan_config_int (config, "MaxEvaluationCost", false, 1000000);
  CONFIG_INT (__RESERVED_MEM_SIZE__) = scan_config_int (config, "ReservedMemorySize", false, 0); /* reserved for emergent shutdown */

  CONFIG_INT (__MAX_ARRAY_SIZE__) = scan_config_int (config, "MaxArraySize", false, 15000);
  CONFIG_INT (__MAX_BUFFER_SIZE__) = scan_config_int (config, "MaxBufferSize", false, 4000000);
  CONFIG_INT (__MAX_MAPPING_SIZE__) = scan_config_int (config, "MaxMappingSize", false, 15000);
  CONFIG_INT (__MAX_STRING_LENGTH__) = scan_config_int (config, "MaxStringLength", false, 200000);
  CONFIG_INT (__MAX_BITFIELD_BITS__) = scan_config_int (config, "MaxBitFieldBits", false, 1200);

  CONFIG_INT (__MAX_BYTE_TRANSFER__) = scan_config_int (config, "MaxByteTransfer", false, 10000);
  CONFIG_INT (__MAX_READ_FILE_SIZE__) = scan_config_int (config, "MaxReadFileSize", false, 20000);
  CONFIG_INT (__SHARED_STRING_HASH_TABLE_SIZE__) = scan_config_int (config, "SharedStringHashSize", false, 20011);
  CONFIG_INT (__OBJECT_HASH_TABLE_SIZE__) = scan_config_int (config, "ObjectHashSize", false, 10007);
  CONFIG_INT (__ENABLE_CRASH_DROP_CORE__) = scan_config_bool (config, "CrashDropCore", false, true);
  CONFIG_INT (__RESOLVER_FORWARD_CACHE_TTL__) = scan_config_int (config, "ResolverForwardCacheTtl", false, 300);
  CONFIG_INT (__RESOLVER_REVERSE_CACHE_TTL__) = scan_config_int (config, "ResolverReverseCacheTtl", false, 900);
  CONFIG_INT (__RESOLVER_NEGATIVE_CACHE_TTL__) = scan_config_int (config, "ResolverNegativeCacheTtl", false, 30);
  CONFIG_INT (__RESOLVER_STALE_REFRESH_WINDOW__) = scan_config_int (config, "ResolverStaleRefreshWindow", false, 30);
  CONFIG_INT (__RESOLVER_FORWARD_QUOTA__) = scan_config_int (config, "ResolverForwardQuota", false, 10);
  CONFIG_INT (__RESOLVER_REVERSE_QUOTA__) = scan_config_int (config, "ResolverReverseQuota", false, 4);
  CONFIG_INT (__RESOLVER_REFRESH_QUOTA__) = scan_config_int (config, "ResolverRefreshQuota", false, 2);
  CONFIG_INT (__EVALUATOR_STACK_SIZE__) = scan_config_int (config, "StackSize", false, 1000);
  CONFIG_INT (__MAX_LOCAL_VARIABLES__) = scan_config_int (config, "MaxLocalVariables", false, 25);
  CONFIG_INT (__MAX_CALL_DEPTH__) = scan_config_int (config, "MaxCallDepth", false, 50);

  if (scan_config_bool (config, "ArgumentsInTrace", false, false))
    g_trace_flag |= DUMP_WITH_ARGS;

  if (scan_config_bool (config, "LocalVariablesInTrace", false, false))
    g_trace_flag |= DUMP_WITH_LOCALVARS;

  /*
   * from options.h
   */
  CONFIG_INT (__COMPILER_STACK_SIZE__) = 200;	/* CFG_COMPILER_STACK_SIZE; */
  CONFIG_INT (__LIVING_HASH_TABLE_SIZE__) = 256;	/* CFG_LIVING_HASH_SIZE; */

  free (config);

  if (fatal_config_error)
    {
      debug_message ("{}\t***** Failed loading config (%d error%s): %s\n",
                     fatal_config_error, fatal_config_error > 1 ? "s":"",
                     config_file);
      exit (EXIT_FAILURE);
    }
}

extern "C"
void init_mudlib_archive(const char* archive_path) {
  if (archive_path && *archive_path)
    {
      /* TODO: Implement mudlib archive initialization */
    }
}

extern "C"
void init_application(const char* master_file) {
  namespace fs = std::filesystem;
  if (master_file && *master_file)
    {
      char path[PATH_MAX];
      if (!realpath (master_file, path))
        {
          perror (master_file);
          exit (EXIT_FAILURE);
        }
      if (CONFIG_STR(__MUD_LIB_DIR__))
        {
          fs::path mudlib_dir(CONFIG_STR(__MUD_LIB_DIR__));
          fs::path master_path(path);

          if (!fs::exists(mudlib_dir))
            {
              debug_message ("{}\t***** Mudlib directory does not exist: %s\n", CONFIG_STR(__MUD_LIB_DIR__));
              exit (EXIT_FAILURE);
            }
          if (!fs::is_directory(mudlib_dir))
            {
              debug_message ("{}\t***** Mudlib directory is not a directory: %s\n", CONFIG_STR(__MUD_LIB_DIR__));
              exit (EXIT_FAILURE);
            }
          while (!fs::equivalent(mudlib_dir, master_path.parent_path()))
            {
              if (master_path == master_path.root_path())
                {
                  debug_message ("{}\t***** Master file is not within MudlibDir: %s\n", path);
                  exit (EXIT_FAILURE);
                }
              master_path = master_path.parent_path();
            }
          SET_CONFIG_STR(__MASTER_FILE__, fs::relative(path, mudlib_dir).string().c_str());
        }
      else
        {
          /* if MudLibDir is not specified, use the parent directory of the master file as MudLibDir */
          fs::path master_path(path);
          fs::path mudlib_dir = master_path.parent_path();
          SET_CONFIG_STR(__MUD_LIB_DIR__, mudlib_dir.string().c_str());
          SET_CONFIG_STR(__MASTER_FILE__, master_path.filename().string().c_str());
        }
    }
}

/**
 * @brief Deinitialize runtime configurations, freeing allocated memory.
 */
extern "C"
void deinit_config() {
  int i;
  /* NOTE: some config_int settings could be changed by LPC programs.
   * for example, set_eval_limit() changes __MAX_EVAL_COST__.
   */
  for (i = 0; i < NUM_CONFIG_INTS; i++)
    config_int[i] = 0;
  for (i = 0; i < NUM_CONFIG_STRS; i++)
    {
      CLEAR_CONFIG_STR(i);
    }
  memset (external_port, 0, sizeof(external_port));
  g_trace_flag = 0;
}

extern "C"
bool get_config_item (svalue_t * res, svalue_t * arg) {
  int num;

  num = (int)arg->u.number;

  if (num < 0 || num >= RUNTIME_CONFIG_NEXT)
    return false;

  if (num >= BASE_CONFIG_INT)
    {
      res->type = T_NUMBER;
      res->u.number = config_int[num - BASE_CONFIG_INT];
    }
  else
    {
      /* config_str is malloc'd but we want LPC treat it as a constant string */
      SET_SVALUE_CONSTANT_STRING(res, config_str[num] ? config_str[num] : "");
    }

  return true;
}
