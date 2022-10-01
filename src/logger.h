#ifndef LOGGER_H
#define LOGGER_H

#include "src/main.h"

/* generic loggers */
extern int log_message (const char* file, const char *fmt, ...);
extern int debug_message (char *, ...);
extern int debug_message_with_src (const char* func, const char* src, int line, const char* fmt, ...);

#if __STDC_VERSION__ >= 199901L
#define debug_fatal(...)		debug_message_with_src(__func__, __FILE__, __LINE__, __VA_ARGS__)
#define debug_error(...)		debug_message_with_src(__func__, __FILE__, __LINE__, __VA_ARGS__)
#define debug_warn(...)			debug_message_with_src(__func__, __FILE__, __LINE__, __VA_ARGS__)
#define debug_info(...)			debug_message_with_src(__func__, __FILE__, __LINE__, __VA_ARGS__)
#define debug_trace(...)		debug_message_with_src(__func__, __FILE__, __LINE__, __VA_ARGS__)
#endif

/* specific error loggers */
extern int debug_perror_with_src (const char* func, const char* src, int line, const char* what, const char* file);
#define debug_perror(what,file)		debug_perror_with_src(__func__, __FILE__, __LINE__, (what), (file))

/* trace loggers */
#define opt_trace(tier, ...)		do{if(SERVER_OPTION(trace_flags)&(tier)) \
					debug_message_with_src(__func__, __FILE__, __LINE__, ## __VA_ARGS__);}while(0)
#define TT_EVAL		01
#define TT_COMPILE	02

#endif /* ! LOGGER_H */
