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

#define opt_error(level, fmt, ...)  do{if(MAIN_OPTION(debug_level)>=(level)) \
   debug_error(fmt, ##__VA_ARGS__);}while(0)
#define opt_warn(level, fmt, ...)   do{if(MAIN_OPTION(debug_level)>=(level)) \
   debug_warn(fmt, ##__VA_ARGS__);}while(0)
#define opt_info(level, fmt, ...)   do{if(MAIN_OPTION(debug_level)>=(level)) \
   debug_notice(fmt, ##__VA_ARGS__);}while(0)

/*
 * Trace levels and tiers are represented as octal values (starting with '0').
 * The lowest 3 bits (0-7) represent the required trace level minus one.
 * The higher bits represent the trace tiers (EVAL, COMPILE, SIMUL_EFUN, BACKEND).
 * 
 * For example:
 * -t 020 enables COMPILE traces at level 0.
 * -t 0107 enables all BACKEND traces (at level 7 and below).
 * -t 052 enables EVAL and SIMUL_EFUN traces at level 2 and below. 
 */
#define TT_LEVEL_MASK   0007U    /* trace level mask (lowest 3 bits) */
#define TT_EVAL         0010U
#define TT_COMPILE      0020U
#define TT_SIMUL_EFUN   0040U
#define TT_BACKEND      0100U
#define TT_COMM         0200U
#define TT_MEMORY       0400U

/**
 * @brief The trace logger macro generates trace log messages based on the tier specification
 *  and the current trace flags set in the server options (specified with -t when starting
 *  the server).
 * @param tier The trace \p tier is a bit-OR'ed value for this trace log:
 *  TT_EVAL = Evaluation related traces.
 *  TT_COMPILE = Compilation related traces.
 *  TT_SIMUL_EFUN = Simul_efun related traces.
 *  TT_BACKEND = Backend related traces.
 *  The lowest 3 bits specify the minimum level of tracing required to log the message.
 * @param fmt The format string for the log message, similar to printf.
 * @param ... The format string and additional arguments for the log message.
 */
#define opt_trace(tier, fmt, ...) do{if((MAIN_OPTION(trace_flags)&(tier)&~TT_LEVEL_MASK) \
   && ((MAIN_OPTION(trace_flags)&TT_LEVEL_MASK) + 1) > ((tier)&TT_LEVEL_MASK)) \
   debug_trace(fmt, ## __VA_ARGS__);}while(0)
