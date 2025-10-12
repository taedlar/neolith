#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char *
xstrdup (const char *s)
{
  char *str;

#ifdef _MSC_VER
  str = _strdup (s);  // ISO C
#else
  str = strdup (s); // POSIX
#endif
  if (!str)
    {
      perror ("strdup");
      abort ();
    }

  return str;
}
