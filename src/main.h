#pragma once

#ifdef __linux__
#include <linux/limits.h>
#endif /* __linux__ */
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define PATH_MAX MAX_PATH
#endif

typedef struct main_options {
  char config_file[PATH_MAX];
  int console_mode;
  int epilog_level;
  int debug_level;
  unsigned long trace_flags;    /* Trace flags for debugging */
} main_options_t;

extern main_options_t* g_main_options;

/* Macros for accessing main options */
#define MAIN_OPTION(x)	(g_main_options->x)
