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

#include "rc.h"
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
#define open    _open
#define fdopen  _fdopen
#define close   _close
#define strcasecmp  _stricmp
#define strncasecmp _strnicmp
#endif

#include "port/wrapper.h"
#include "port/debug.h"

int g_trace_flag = 0;

char *config_str[NUM_CONFIG_STRS]; /* NULL or malloc'd strings */
int config_int[NUM_CONFIG_INTS];

port_def_t external_port[5];

/* static declarations */

static int fatal_config_error = 0;

static char *read_config_malloc (const char *filename);
static char *scan_config (char *config, const char *name, int required, char *def);
static int scan_config_i (char *config, const char *name, int required, int def);
static int scan_config_b (char *config, const char *name, int required, int def);

/* implementations */

static char *
read_config_malloc (const char *filename)
{
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
 *  @param name The name of the configuration item to search for.
 *  @param required Flag indicating if the configuration item is required.
 *    if required > 0, it is mandatory; if required == 0, it is optional;
 *    if required < 0, it is optional with a warning if missing.
 *  @param def Default value to return if the item is not found.
 *  @return The extracted configuration value (allocated) or the default value.
 */
static char *
scan_config (char *config, const char *name, int required, char *def)
{
  int sz_name;
  char *term, *p;

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

  // name not found
  if (required < 0)
    {
      debug_warn ("{}\t%s is missing, assuming %s", name, def ? def : "null");
      return def;
    }

  if (required > 0)
    {
      debug_error ("%s is missing", name);
      fatal_config_error++;
      return NULL;
    }

  return def;
}

static int
scan_config_i (char *config, const char *name, int required, int def)
{
  char *term;
  int n;

  if (NULL == (term = scan_config (config, name, required, NULL)))
    return def;

  n = atoi (term);
  free (term);

  return n;
}

static int
scan_config_b (char *config, const char *name, int required, int def)
{
  char *p;
  int result = def;

  while ((p = scan_config (config, name, required, NULL)))
    {
      if (!strcasecmp (p, "yes"))
        result = 1;
      else if (!strcasecmp (p, "no"))
        result = 0;
      else
        debug_message ("warnning: %s must be 'Yes' or 'No' [got: %s]\n", name, p);
      free (p);
      required = 0;
    }

  return result;
}

extern "C" void init_config (const char *config_file)
{
  int i;
  char *config, *p;

  for (i = 0; i < NUM_CONFIG_INTS; i++)
    config_int[i] = 0;
  for (i = 0; i < NUM_CONFIG_STRS; i++)
    config_str[i] = 0;

  if (NULL == (config = read_config_malloc (config_file)))
    {
      perror (config_file);
      if (errno == ENOENT)
        fprintf (stderr, "Use -f option to specify a config file\n");
      exit (EXIT_FAILURE);
    }

  CONFIG_STR (__LOG_DIR__) = scan_config (config, "LogDir", 0, NULL);
  if (CONFIG_STR (__LOG_DIR__))
    {
      // we only assign DebugLogFile if LogDir is specified because the debug log file
      // is independent from the mudlib directory. If LogDir is not specified, we'll
      // ignore the DebugLogFile setting and write debug logs directly to standard error.
      //
      // This guarantees that debug logs are always written to the console when LogDir
      // is not set, which is useful for a read-only mudlib environment.
      CONFIG_STR (__DEBUG_LOG_FILE__) = scan_config (config, "DebugLogFile", 0, NULL);
    }
  else
    {
      fprintf (stderr, "LogDir is not specified, debug logs will be written to standard error\n");
    }
  CONFIG_INT (__ENABLE_LOG_DATE__) = scan_config_b (config, "LogWithDate", 0, 0);

  CONFIG_STR (__MUD_LIB_DIR__) = scan_config (config, "MudlibDir", 1, NULL); // required
  CONFIG_STR (__MUD_NAME__) = scan_config (config, "MudName", 0, NULL);
  CONFIG_STR (__ADDR_SERVER_IP__) = scan_config (config, "AddrServerIP", 0, NULL);

  CONFIG_STR (__GLOBAL_INCLUDE_FILE__) = scan_config (config, "GlobalInclude", 0, NULL);
  CONFIG_STR (__BIN_DIR__) = NULL;
  CONFIG_STR (__INCLUDE_DIRS__) = scan_config (config, "IncludeDir", 0, NULL);
  CONFIG_STR (__SAVE_BINARIES_DIR__) = scan_config (config, "SaveBinaryDir", 0, NULL);
  CONFIG_STR (__MASTER_FILE__) = scan_config (config, "MasterFile", 1, NULL); // required
  CONFIG_STR (__SIMUL_EFUN_FILE__) = scan_config (config, "SimulEfunFile", 0, NULL);
  CONFIG_STR (__DEFAULT_ERROR_MESSAGE__) = scan_config (config, "DefaultErrorMsg", 0, NULL);
  CONFIG_STR (__DEFAULT_FAIL_MESSAGE__) = scan_config (config, "DefaultFailMsg", 0, NULL);

  for (i = 0; i < 5 && (p = scan_config (config, "Port", 0, NULL)); i++)
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

  CONFIG_INT (__ADDR_SERVER_PORT__) = scan_config_i (config, "AddrServerPort", 0, 0);
  CONFIG_INT (__TIME_TO_CLEAN_UP__) = scan_config_i (config, "CleanupDuration", 0, 600);
  CONFIG_INT (__TIME_TO_RESET__) = scan_config_i (config, "ResetDuration", 0, 1800);
  CONFIG_INT (__INHERIT_CHAIN_SIZE__) = scan_config_i (config, "MaxInheritDepth", 0, 30);
  CONFIG_INT (__MAX_EVAL_COST__) = scan_config_i (config, "MaxEvaluationCost", 0, 1000000);
  CONFIG_INT (__RESERVED_MEM_SIZE__) = scan_config_i (config, "ReservedMemorySize", 0, 0); /* reserved for emergent shutdown */

  CONFIG_INT (__MAX_ARRAY_SIZE__) = scan_config_i (config, "MaxArraySize", 0, 15000);
  CONFIG_INT (__MAX_BUFFER_SIZE__) = scan_config_i (config, "MaxBufferSize", 0, 4000000);
  CONFIG_INT (__MAX_MAPPING_SIZE__) = scan_config_i (config, "MaxMappingSize", 0, 15000);
  CONFIG_INT (__MAX_STRING_LENGTH__) = scan_config_i (config, "MaxStringLength", 0, 200000);
  CONFIG_INT (__MAX_BITFIELD_BITS__) = scan_config_i (config, "MaxBitFieldBits", 0, 1200);

  CONFIG_INT (__MAX_BYTE_TRANSFER__) = scan_config_i (config, "MaxByteTransfer", 0, 10000);
  CONFIG_INT (__MAX_READ_FILE_SIZE__) = scan_config_i (config, "MaxReadFileSize", 0, 20000);
  CONFIG_INT (__SHARED_STRING_HASH_TABLE_SIZE__) = scan_config_i (config, "SharedStringHashSize", 0, 20011);
  CONFIG_INT (__OBJECT_HASH_TABLE_SIZE__) = scan_config_i (config, "ObjectHashSize", 0, 10007);
  CONFIG_INT (__ENABLE_CRASH_DROP_CORE__) = scan_config_b (config, "CrashDropCore", 0, 1);
  CONFIG_INT (__EVALUATOR_STACK_SIZE__) = scan_config_i (config, "StackSize", 0, 1000);
  CONFIG_INT (__MAX_LOCAL_VARIABLES__) = scan_config_i (config, "MaxLocalVariables", 0, 25);
  CONFIG_INT (__MAX_CALL_DEPTH__) = scan_config_i (config, "MaxCallDepth", 0, 50);

  if (scan_config_b (config, "ArgumentsInTrace", 0, 0))
    g_trace_flag |= DUMP_WITH_ARGS;

  if (scan_config_b (config, "LocalVariablesInTrace", 0, 0))
    g_trace_flag |= DUMP_WITH_LOCALVARS;

  /*
   * from options.h
   */
  CONFIG_INT (__COMPILER_STACK_SIZE__) = 200;	/* CFG_COMPILER_STACK_SIZE; */
  CONFIG_INT (__LIVING_HASH_TABLE_SIZE__) = 256;	/* CFG_LIVING_HASH_SIZE; */

  free (config);

  if (fatal_config_error)
    {
      debug_message ("*****Failed loading config (%d error%s): %s\n",
                     fatal_config_error, fatal_config_error > 1 ? "s":"",
                     config_file);
      exit (EXIT_FAILURE);
    }
}

/**
 * @brief Deinitialize runtime configurations, freeing allocated memory.
 */
extern "C" void deinit_config() {
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

extern "C" int get_config_item (svalue_t * res, svalue_t * arg)
{
  int num;

  num = arg->u.number;

  if (num < 0 || num >= RUNTIME_CONFIG_NEXT)
    return 0;

  if (num >= BASE_CONFIG_INT)
    {
      res->type = T_NUMBER;
      res->u.number = config_int[num - BASE_CONFIG_INT];
    }
  else
    {
      res->type = T_STRING;
      res->subtype = STRING_CONSTANT; /* prevent deallocation */
      res->u.string = config_str[num] ? config_str[num] : (char*)"";
    }

  return 1;
}
