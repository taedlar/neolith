#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "wrapper.h"

int xvasprintf(char **strp, const char *fmt, va_list ap) {
#ifdef _GNU_SOURCE
  return vasprintf(strp, fmt, ap);
#else
  va_list ap_copy;
  int needed;
  int written;

  if (!strp)
    return -1;

  va_copy(ap_copy, ap);
  needed = vsnprintf(NULL, 0, fmt, ap_copy);
  va_end(ap_copy);

  if (needed < 0)
    {
      *strp = NULL;
      return -1;
    }

  *strp = (char *)malloc((size_t)needed + 1);
  if (!*strp)
    return -1;

  written = vsnprintf(*strp, (size_t)needed + 1, fmt, ap);
  if (written < 0)
    {
      free(*strp);
      *strp = NULL;
      return -1;
    }

  return written;
#endif
}
