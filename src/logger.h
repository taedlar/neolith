#ifndef LOGGER_H
#define LOGGER_H

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

#endif /* ! LOGGER_H */
