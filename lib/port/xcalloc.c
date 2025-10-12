#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdlib.h>

void *
xcalloc (size_t nmemb, size_t size)
{
  void *p;

  p = calloc (nmemb, size);
  if (!p)
    {
      perror ("calloc");
      abort ();
    }

  return p;
}
