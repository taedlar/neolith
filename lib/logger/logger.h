#pragma once
#include <stdio.h>
#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#else
typedef int bool;
#define true 1
#define false 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern FILE* current_log_file;
extern int current_log_severity;

/**
  @brief Print raw log messages to file or previously opend file (if file is NULL).
  @param file The log file path. If NULL, write to the previously opened log file.
              If empty string, write to stderr.
  @param fmt The format string.
  @return The number of characters written, or a negative value if an error occurs.
 */
int log_message (const char* file, const char *fmt, ...);

/**
 * @brief Set the debug log file.
 * @param filename The path to the log file.
 */
void debug_set_log_file (const char* filename);

/**
 * @brief Set whether to prepend date/time to debug messages.
 * @param enable Non-zero to enable date/time prepending, zero to disable.
 */
void debug_set_log_with_date (bool enable);

/**
 * @brief Set the global value \p current_log_severity for debug macros to use.
 *   The logger only store the debug log severity for cross-module use (default = -1).
 *   The debug macros should check if it is writing a message with severity greater than
 *   this value before writing.
 * @param severity The log severity to set.
 */
void debug_set_log_severity (int severity);

/**
 * @brief Log a debug message.
 * @param fmt The format string.
 * @return The number of characters written, or a negative value if an error occurs.
 */
int debug_message (const char* fmt, ...);

/* debug macro helpers */
int debug_log_with_src (const char* log_type, const char* func, const char* src, int line, const char* fmt, ...);
int debug_perror_with_src (const char* func, const char* src, int line, const char* what, const char* file);

#ifdef __cplusplus
}
#endif
