#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#define NO_OPCODES
#include "std.h"

char *reserved_area = NULL;		/* reserved for MALLOC() */

char *
xalloc (size_t size)
{
  char *p;
  static int going_to_exit = 0;

  if (going_to_exit)
    exit (3);
  p = (char *) DMALLOC (size, TAG_MISC, "main.c: xalloc");
  if (p == 0)
    {
      if (reserved_area)
        {
          FREE (reserved_area);
          /* after freeing reserved area, we are supposed to be able to write log messages */
          debug_message ("{}\t***** temporarily out of MEMORY. Freeing reserve.");
          reserved_area = 0;
          slow_shut_down_to_do = 6;
          return xalloc (size);	/* Try again */
        }
      going_to_exit = 1;
      fatal ("Totally out of MEMORY.\n");
    }
  return p;
}

#ifdef DO_MSTATS
void
show_mstats (outbuffer_t * ob, char *s)
{
  outbuf_add (ob, "No malloc statistics available with SYSMALLOC\n");
}
#endif
