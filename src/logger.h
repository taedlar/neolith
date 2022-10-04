#ifndef LOGGER_H
#define LOGGER_H

#ifdef STD_HEADERS
#include <stdio.h>
#endif

extern FILE* current_log_file;

extern int log_message (const char* file, const char *fmt, ...);
extern int debug_message (const char* fmt, ...);
extern int debug_message_with_src (const char* _type, const char* func, const char* src, int line, const char* fmt, ...);

extern int debug_perror_with_src (const char* func, const char* src, int line, const char* what, const char* file);

#endif /* ! LOGGER_H */
