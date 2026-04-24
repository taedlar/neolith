#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <string.h>
#include "wrapper.h"

/**
 * @brief A safe string copy function that prevents buffer overflows.
 * 
 * Copy the string \p src to \p dest, but do not write more than \p end - \p dest
 * characters (including the terminating null character). If \p src is too long to fit,
 * the result is truncated and null-terminated.
 * 
 * @param dest The destination buffer to copy to.
 * @param end The end of the destination buffer (i.e., dest + buffer_size).
 * @param src The source string to copy from.
 * @return A pointer to the null terminator in the destination buffer after copying.
 *      If \p dest is already at or past \p end, no copying is done and \p dest is returned
 *      and the null terminator is not written.
 */
char *strput (char *dest, char *end, const char *src) {
  if (dest >= end)
    return dest;
  while ((*dest++ = *src++))
    {
      if (dest == end)
        {
          *(dest - 1) = 0; /* truncate */
          break;
        }
    }
  return dest - 1;
}

char* strput_int (char *dest, char *end, int num) {
  char buf[20];
  sprintf (buf, "%d", num);
  return strput (dest, end, buf);
}
