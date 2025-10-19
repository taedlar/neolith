#pragma once
#include <stdio.h>

extern FILE* current_log_file;

#ifdef __cplusplus
extern "C" {
#endif

int log_message (const char* file, const char *fmt, ...);

void debug_set_log_file (const char* filename);
void debug_set_log_with_date (int enable);

int debug_message (const char* fmt, ...);
int debug_message_with_src (const char* _type, const char* func, const char* src, int line, const char* fmt, ...);
int debug_perror_with_src (const char* func, const char* src, int line, const char* what, const char* file);

#ifdef __cplusplus
}
#endif
