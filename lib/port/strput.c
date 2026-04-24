#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <string.h>
#include "wrapper.h"

char *strput (char *x, char *limit, const char *y) {
#ifdef HAVE_STPNCPY
  return stpncpy(x, y, limit - x);
#else
  while ((*x++ = *y++))
    {
      if (x == limit)
        {
          *(x - 1) = 0;
          break;
        }
    }
  return x - 1;
#endif
}

char* strput_int (char *x, char *limit, int num) {
  char buf[20];
  sprintf (buf, "%d", num);
  return strput (x, limit, buf);
}
