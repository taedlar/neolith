/*  $Id: qsort.c,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

/* median-3 variant of quicksort - coded by John Garnett.

   using this quicksort rather than the builtin one because most
   builtin implementations choke on non-deterministic compare functions
   (and we can't control what compare function is used since it is at
   the mudlib level).  Based on algorithm appearing in _Data Structures and
   Algorithm Analysis_ by Cawnthorpe.
*/

#include "qsort.h"

#define LEN sizeof(svalue_t)
#define MAX_LEN 1000

static void doSwap (char *, char *, int);
static void qSort (void *, int, int, int, int, int (*)());

static void
doSwap (register char *one, register char *two, int size)
{
  register char t;

  while (size--)
    {
      t = *one;
      *(one++) = *two;
      *(two++) = t;
    }
}

/* qsort adapted from page 87 of K&R 2nd edition */

static void
qSort (void *v, int left, int right, int size, int rightmost,
       int (*compar) (void *, void *))
{
  int i, last, szleft;

  if ((left >= right) || (left < 0) || (right > rightmost) || (right < 0))
    {
      return;
    }
  szleft = size * left;
  doSwap ((char *) v + szleft, (char *) v + (size * ((left + right) / 2)),
	  size);
  last = left;
  for (i = left + 1; i <= right; i++)
    {
      if ((*compar) ((char *) v + (size * i), (char *) v + szleft) < 0)
	{
	  doSwap ((char *) v + (size * ++last), (char *) v + (size * i),
		  size);
	}
    }
  doSwap ((char *) v + szleft, (char *) v + (size * last), size);
  qSort (v, left, last - 1, size, rightmost, compar);
  qSort (v, last + 1, right, size, rightmost, compar);
}

void
quickSort (void *a, int nmemb, int size, int (*compar) (void *, void *))
{
  if (nmemb < 2)
    {
      return;
    }
  qSort (a, 0, nmemb - 1, size, nmemb - 1, compar);
}
