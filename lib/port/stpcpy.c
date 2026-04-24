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
