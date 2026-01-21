#pragma once

#ifdef __linux__
#include <linux/limits.h>
#endif /* __linux__ */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define PATH_MAX MAX_PATH
#endif

typedef struct main_options {
  char config_file[PATH_MAX];   /* -f, --config-file */
  int console_mode;             /* -c, --console-mode */
  int pedantic;                 /* -p, --pedantic */
  int epilog_level;             /* -e, --epilog-level */
  int debug_level;              /* -d, --debug-level */
  unsigned long trace_flags;    /* -t, --trace-flags */
  unsigned int timer_flags;     /* -r, --timers */
} main_options_t;

extern main_options_t* g_main_options;

/* Macros for accessing main options */
#define MAIN_OPTION(x)	(g_main_options->x)

#define TIMER_FLAG_RESET       0x01  /* Enable reset timer */
#define TIMER_FLAG_HEARTBEAT   0x02  /* Enable heart_beat timer */
#define TIMER_FLAG_CALLOUT     0x04  /* Enable call_out timer */