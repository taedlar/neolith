#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "logger.h"
#include "rc.h"

FILE* current_log_file = NULL;

/**
  @brief Print raw log messages to file or previously opend file (if file is NULL).
  @param file The log file path. If NULL, write to the previously opened log file.
              If empty string, write to stderr.
  @param fmt The format string.
  @return The number of characters written, or a negative value if an error occurs.
 */
int log_message (const char *file, const char *fmt, ...)
{
  int n_written = 0;
  static char current_log_filename[PATH_MAX] = "";
  va_list args;

  if (file && (0 != strcmp(file, current_log_filename))) /* writing to a different log file? */
    {
      if (current_log_file && (current_log_file != stderr))
	      fclose (current_log_file);
      strncpy (current_log_filename, file, sizeof(current_log_filename) - 1);
      current_log_filename[sizeof(current_log_filename) - 1] = 0;

      if (*file)
        {
	        current_log_file = fopen (file, "a"); /* append mode */
	        if (!current_log_file)
	          {
	            current_log_file = stderr;
	            n_written = debug_message ("{}\t***** error opening log file %s: %s\n", file, strerror (errno));
	          }
	      }
      else
	      current_log_file = stderr;
    }

  if (!current_log_file)
    current_log_file = stderr;  /* fallback */

  va_start (args, fmt);
  if (*fmt && (n_written >= 0))
    {
      int ret = vfprintf (current_log_file, fmt, args);
      if (ret > 0)
	      n_written += ret;
    }
  va_end (args);

  fflush (current_log_file);
  return n_written;
}

/**
 * @brief Log a debug message.
 * @param fmt The format string.
 * @return The number of characters written, or a negative value if an error occurs.
 */
int debug_message (const char *fmt, ...)
{
  static char debug_log_file[PATH_MAX] = "";
  va_list args;
  int n_written = 0;
  char msg[8192];	/* error message cannot exceed this size */

  if (!*debug_log_file) /* first time called */
    {
      if (CONFIG_STR (__DEBUG_LOG_FILE__))
      	{
	        if (CONFIG_STR (__LOG_DIR__))
	          snprintf (debug_log_file, sizeof(debug_log_file), "%s/%s", CONFIG_STR (__LOG_DIR__), CONFIG_STR (__DEBUG_LOG_FILE__));
	        else
	          snprintf (debug_log_file, sizeof(debug_log_file), "%s", CONFIG_STR (__DEBUG_LOG_FILE__));
	      }
      debug_log_file[sizeof(debug_log_file) - 1] = 0;
    }

  va_start (args, fmt);
  vsnprintf (msg, sizeof(msg), fmt, args); /* truncated if too long */
  va_end (args);

  /* replace newlines and carriage returns with spaces (utf-8 assumed) */
  for (size_t i = 0; msg[i] != 0; i++)
    {
      if (msg[i] == '\r' || msg[i] == '\n')
        msg[i] = ' ';
    }

  if (CONFIG_INT (__ENABLE_LOG_DATE__))
    {
      char time_info[1024];
      time_t t = time(NULL);
      struct tm* now = localtime (&t);
      strftime (time_info, sizeof(time_info), "%G-%m-%d %T", now);  /* ISO 8601 format */
      n_written = log_message(debug_log_file, "%s\t%s\n", time_info, msg);
    }
  else
    {
      n_written = log_message(debug_log_file, "%s\n", msg);
    }

  return n_written;
}

int
debug_message_with_src (const char* _type, const char* func, const char* src, int line, const char *fmt, ...)
{
    va_list args;
    char msg[8192];
    size_t n = snprintf(msg, sizeof(msg), "[\"%s\",\"%s\",%d,\"%s\"]\t", _type, src, line, func);

    if (n < sizeof(msg)) {
        va_start (args, fmt);
        vsnprintf (msg + n, sizeof(msg) - n, fmt, args);
    }
    return debug_message ("%s", msg);
}

int
debug_perror_with_src (const char* func, const char* src, int line, const char *what, const char *file)
{
  if (file)
    return debug_message ("[\"ERROR\",\"%s\",%d,\"%s\"]\t%s: [%s] %s\n", src, line, func, what, file, strerror (errno));
  else
    return debug_message ("[\"ERROR\",\"%s\",%d,\"%s\"]\t%s: %s\n", src, line, func, what, strerror (errno));
}


