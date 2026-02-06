#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"

void outbuf_zero (outbuffer_t * outbuf) {
  outbuf->real_size = 0;
  outbuf->buffer = 0;
}

size_t outbuf_extend (outbuffer_t * outbuf, size_t len) {
  size_t limit;

  if (outbuf->buffer)
    {
      limit = MSTR_SIZE (outbuf->buffer);
      if (outbuf->real_size + len > limit)
        {
          if (outbuf->real_size == USHRT_MAX)
            return 0;		/* TRUNCATED */

          /* assume it's going to grow some more */
          limit = (outbuf->real_size + len) * 2;
          if (limit > USHRT_MAX)
            {
              limit = outbuf->real_size + len;
              if (limit > USHRT_MAX)
                {
                  outbuf->buffer = extend_string (outbuf->buffer, USHRT_MAX);
                  return USHRT_MAX - outbuf->real_size;
                }
            }
          outbuf->buffer = extend_string (outbuf->buffer, limit);
        }
    }
  else
    {
      outbuf->buffer = new_string (len, "outbuf_add");
      outbuf->real_size = 0;
    }
  return len;
}

void outbuf_add (outbuffer_t * outbuf, const char *str) {
  size_t len, limit;

  if (!outbuf)
    return;
  len = strlen (str);
  if (outbuf->buffer)
    {
      limit = MSTR_SIZE (outbuf->buffer);
      if (outbuf->real_size + len > limit)
        {
          if (outbuf->real_size == USHRT_MAX)
            return;		/* TRUNCATED */

          /* assume it's going to grow some more */
          limit = (outbuf->real_size + len) * 2;
          if (limit > USHRT_MAX)
            {
              limit = outbuf->real_size + len;
              if (limit > USHRT_MAX)
                {
                  outbuf->buffer = extend_string (outbuf->buffer, USHRT_MAX);
                  strncpy (outbuf->buffer + outbuf->real_size, str,
                           USHRT_MAX - outbuf->real_size);
                  outbuf->buffer[USHRT_MAX] = 0;
                  outbuf->real_size = USHRT_MAX;
                  return;
                }
            }
          outbuf->buffer = extend_string (outbuf->buffer, limit);
        }
    }
  else
    {
      outbuf->buffer = new_string (len, "outbuf_add");
      outbuf->real_size = 0;
    }
  strcpy (outbuf->buffer + outbuf->real_size, str);
  outbuf->real_size += len;
}

void outbuf_addchar (outbuffer_t * outbuf, char c) {
  size_t limit;

  if (!outbuf)
    return;

  if (outbuf->buffer)
    {
      limit = MSTR_SIZE (outbuf->buffer);
      if (outbuf->real_size + 1 > limit)
        {
          if (outbuf->real_size == USHRT_MAX)
            return;		/* TRUNCATED */

          /* assume it's going to grow some more */
          limit = (outbuf->real_size + 1) * 2;
          if (limit > USHRT_MAX)
            {
              limit = outbuf->real_size + 1;
              if (limit > USHRT_MAX)
                {
                  outbuf->buffer = extend_string (outbuf->buffer, USHRT_MAX);
                  *(outbuf->buffer + outbuf->real_size) = c;
                  outbuf->buffer[USHRT_MAX] = 0;
                  outbuf->real_size = USHRT_MAX;
                  return;
                }
            }
          outbuf->buffer = extend_string (outbuf->buffer, limit);
        }
    }
  else
    {
      outbuf->buffer = new_string (80, "outbuf_add");
      outbuf->real_size = 0;
    }
  *(outbuf->buffer + outbuf->real_size++) = c;
  *(outbuf->buffer + outbuf->real_size) = 0;
}

void outbuf_addv (outbuffer_t * outbuf, const char *format, ...) {
  char buf[LARGEST_PRINTABLE_STRING];
  va_list args;

  va_start (args, format);

  vsprintf (buf, format, args);
  va_end (args);

  if (!outbuf)
    return;

  outbuf_add (outbuf, buf);
}

void outbuf_fix (outbuffer_t * outbuf) {
  if (outbuf && outbuf->buffer)
    outbuf->buffer = extend_string (outbuf->buffer, outbuf->real_size);
}

void outbuf_push (outbuffer_t * outbuf) {
  (++sp)->type = T_STRING;
  if (outbuf && outbuf->buffer)
    {
      outbuf->buffer = extend_string (outbuf->buffer, outbuf->real_size);

      sp->subtype = STRING_MALLOC;
      sp->u.string = outbuf->buffer;
    }
  else
    {
      sp->subtype = STRING_CONSTANT;
      sp->u.string = "";
    }
}
