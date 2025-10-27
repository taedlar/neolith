#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include "wrapper.h"

#ifndef	HAVE_STPCPY
char* stpcpy (char *dest, const char *src) {
  while (*src)
    *dest++ = *src++;
  return dest;
}
#endif /* ! HAVE_STPCPY */

#ifndef	HAVE_STPNCPY
char* stpncpy (char *dest, const char *src, size_t n) {
  size_t i;
  for (i = 0; i < n && src[i]; i++)
    dest[i] = src[i];
  if (i < n)
    dest[i] = '\0'; /* POSIX-1.2008 requires exactly n bytes copied, but we don't rely on it */
  return dest + i;
}
#endif	/* ! HAVE_STPNCPY */
