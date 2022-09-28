/*  $Id: wrapper.c,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef	STDC_HEADERS
#include <string.h>
#include <stdlib.h>
#endif /* STDC_HEADERS */

#include "wrapper.h"

/* implementations */

char *
xstrdup (char *s)
{
  char *str;

  str = strdup (s);
  if (!str)
    {
      perror ("strdup");
      abort ();
    }

  return str;
}

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

#ifndef	HAVE_STPCPY
char *
stpcpy (char *dest, const char *src)
{
  while (*src)
    *dest++ = *src++;
  return dest;
}
#endif /* ! HAVE_STPCPY */
