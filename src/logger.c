#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef	STDC_HEADERS
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#endif /* STDC_HEADERS */

#include "rc.h"

/* log_message() - Print log messages to file or previously opend file (if file is NULL). */
int
log_message (const char *file, const char *fmt, ...)
{
  static char fname[PATH_MAX];
  static FILE *fp = NULL;
  va_list args;

  if (file)
    {
      if (fp)
	{
	  fclose (fp);
	  fp = NULL;
	}
      strncpy (fname, file, sizeof(fname)-1);

      if (!*fname)
	return 0;

      fp = fopen (fname, "a");
      if (!fp && (errno == EMFILE || errno == ENFILE))
	fp = freopen (fname, "a", stdout);
      if (!fp)
	{
	  fprintf (stderr, _("error opening log file %s: %s\n"),
		fname, strerror (errno));
	  return 0;	/* failed */
	}
    }

  if (!fp)
    return 0;	/* fail */

  va_start (args, fmt);
  vfprintf (fp, fmt, args);
  va_end (args);

  fflush (fp);

  /* flush, but leave fp open until next call to log_mesage */

  return 1;	/* success */
}

int
debug_message (char *fmt, ...)
{
  static int append = 0;
  static char filename[PATH_MAX], *fname = NULL;
  va_list args;
  char time_info[1024];
  struct tm *now;
  time_t t;
  char msg[8192];	/* error message cannot exceed this size */
  char* ptr;
  int res;

  if (!append)
    {
      memset (filename, 0, sizeof (filename));
      fname = filename;
      if (CONFIG_STR (__DEBUG_LOG_FILE__))
	{
	  if (CONFIG_STR (__LOG_DIR__))
	    snprintf (fname, PATH_MAX, "%s/%s",
		      CONFIG_STR (__LOG_DIR__),
		      CONFIG_STR (__DEBUG_LOG_FILE__));
	  else
	    snprintf (fname, PATH_MAX, "%s", CONFIG_STR (__DEBUG_LOG_FILE__));
	}
      append = 1;
    }

  va_start (args, fmt);
  vsnprintf (msg, sizeof(msg), fmt, args); /* truncated if too long */

  /* since we offer the option to prefix datetime for each debug message, we should make
   * sure each debug message is terminated by one and only one newline. */
  ptr = strchr (msg, '\n');
  while (ptr) {
    *ptr = ' ';
    ptr = strchr (ptr, '\n');
  }

  if (CONFIG_INT (__ENABLE_LOG_DATE__))
    {
      time (&t);
      now = localtime (&t);
      strftime (time_info, 1024, "%G-%m-%d %T\t", now);
      res = log_message (fname, time_info) && log_message (NULL, "%s\n", msg);
    }
  else
    res = log_message (fname, "%s\n", msg);
  va_end (args);

  return res;
}

int
debug_message_with_src (const char* _type, const char* func, const char* src, int line, const char *fmt, ...)
{
    va_list args;
    char msg[8192];
    int n;

    n = snprintf(msg, sizeof(msg), "[\"%s\",\"%s\",%d,\"%s\"]\t", _type, src, line, func);

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


