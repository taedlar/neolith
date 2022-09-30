#ifndef LOGGER_H
#define LOGGER_H

/* generic loggers */
extern int log_message (const char* file, const char *fmt, ...);
extern int debug_message (char *, ...);
extern int debug_message_with_src (const char* func, const char* src, int line, const char* fmt, ...);

#define debug_trace(what)		debug_message_with_src(__func__, __FILE__, __LINE__, "%s", (what))

/* error loggers */
extern int debug_perror_with_src (const char* func, const char* src, int line, const char* what, const char* file);
#define debug_perror(what,file)		debug_perror_with_src(__func__, __FILE__, __LINE__, (what), (file))

#endif /* ! LOGGER_H */
