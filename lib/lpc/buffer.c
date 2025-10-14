/*  $Id: buffer.c,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith

    ORIGINAL AUTHOR
	[1993-11-07] by Garnett for MudOS 0.9.x

    MODIFIED BY
	[2001-06-27] by Annihilator <annihilator@muds.net>
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "types.h"
#include "buffer.h"
#include "lpc/include/runtime_config.h"
#include "src/rc.h"
#include "src/simulate.h"

buffer_t null_buf = {
  1,				/* Ref count, which will ensure that it will
				 * never be deallocated */
  0				/* size */
};

inline buffer_t *
null_buffer ()
{
  null_buf.ref++;
  return &null_buf;
}				/* null_buffer() */

inline void
free_buffer (buffer_t * b)
{
  b->ref--;
  /* don't try to free the null_buffer (ref count might overflow) */
  if ((b->ref > 0) || (b == &null_buf))
    {
      return;
    }
  FREE ((char *) b);
}				/* free_buffer() */

buffer_t *
allocate_buffer (int size)
{
  buffer_t *buf;

#ifndef DISALLOW_BUFFER_TYPE
  if ((size < 0) || (size > CONFIG_INT (__MAX_BUFFER_SIZE__)))
    {
      error ("Illegal buffer size.\n");
    }
  if (size == 0)
    {
      return null_buffer ();
    }
  /* using calloc() so that memory will be zero'd out when allocated */
  buf = (buffer_t *) DCALLOC (sizeof (buffer_t) + size - 1, 1,
			      TAG_BUFFER, "allocate_buffer");
  buf->size = size;
  buf->ref = 1;
  return buf;
#else
  return NULL;
#endif
}

int
write_buffer (buffer_t * buf, int start, char *str, int theLength)
{
  int size;

  size = buf->size;
  if (start < 0)
    {
      start = size + start;
      if (start < 0)
	{
	  return 0;
	}
    }
  /*
   * can't write past the end of the buffer since we can't reallocate the
   * buffer here (no easy way to propagate back the changes to the caller
   */
  if ((start + theLength) > size)
    {
      return 0;
    }
  memcpy (buf->item + start, str, theLength);
  return 1;
}				/* write_buffer() */

char *
read_buffer (buffer_t * b, int start, int len, int *rlen)
{
  char *str;
  unsigned int size;

  if (len < 0)
    return 0;

  size = b->size;
  if (start < 0)
    {
      start = size + start;
      if (start < 0)
	{
	  return 0;
	}
    }
  if (len == 0)
    {
      len = size;
    }
  if (start >= size)
    {
      return 0;
    }
  if ((start + len) > size)
    {
      len = (size - start);
    }
  for (str = (char *) b->item + start, size = 0; *str && size < len;
       str++, size++)
    ;
  str = new_string (size, "read_buffer: str");
  memcpy (str, b->item + start, size);
  str[*rlen = size] = '\0';

  return str;
}				/* read_buffer() */
