#pragma once

#ifdef __linux__
#include <linux/limits.h>
#endif /* __linux__ */
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define PATH_MAX MAX_PATH
#endif

typedef struct server_options
{
  char config_file[PATH_MAX];
  int debug_level;
  unsigned long trace_flags;  
} server_options_t;

extern server_options_t* g_svropts;

#define SERVER_OPTION(x)	(g_svropts->x)
