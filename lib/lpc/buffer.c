/*
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
#include "rc.h"

buffer_t null_buf = {
  .ref = 1,	/* Ref count, which will ensure that it will never be deallocated */
  .size = 0	/* size */
};

buffer_t *null_buffer ()
{
  null_buf.ref++;
  return &null_buf;
}				/* null_buffer() */

void free_buffer (buffer_t * b)
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
allocate_buffer (size_t size)
{
  buffer_t *buf;

#ifndef DISALLOW_BUFFER_TYPE
  if (size > (size_t)CONFIG_INT (__MAX_BUFFER_SIZE__))
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
  buf->size = (unsigned short)size;
  buf->ref = 1;
  return buf;
#else
  return NULL;
#endif
}

int
write_buffer (buffer_t * buf, long start, char *str, size_t theLength)
{
  size_t size;

  size = buf->size;
  if (start < 0)
    {
      start = (long)size + start;
      if (start < 0)
        {
          return 0;
        }
    }
  /*
   * can't write past the end of the buffer since we can't reallocate the
   * buffer here (no easy way to propagate back the changes to the caller
   */
  if ((size_t)start + theLength > size)
    {
      return 0;
    }
  memcpy (buf->item + start, str, theLength);
  return 1;
}				/* write_buffer() */

char* read_buffer (buffer_t * b, long start, size_t len, size_t *rlen) {
  char *str;
  size_t size;

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
  if ((size_t)start >= size)
    {
      return 0;
    }
  if ((size_t)start + len > size)
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
