#pragma once

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct main_options {
  char config_file[PATH_MAX];   /* -f, --config-file */
  bool console_mode;            /* -c, --console-mode */
  bool pedantic;                /* -p, --pedantic */
  int epilog_level;             /* -e, --epilog-level */
  int debug_level;              /* -d, --debug-level */
  unsigned long trace_flags;    /* -t, --trace-flags */
  unsigned int timer_flags;     /* -r, --timers */

  char mud_app[PATH_MAX];   /* master file or mudlib archive, from command line */
} main_options_t;

extern main_options_t* g_main_options;

/* Macros for accessing main options */
#define MAIN_OPTION(x)	(g_main_options->x)

#define TIMER_FLAG_RESET       0x01  /* Enable reset timer */
#define TIMER_FLAG_HEARTBEAT   0x02  /* Enable heart_beat timer */
#define TIMER_FLAG_CALLOUT     0x04  /* Enable call_out timer */

#ifdef __cplusplus
}
#endif
