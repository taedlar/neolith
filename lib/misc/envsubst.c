#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "envsubst.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#ifdef HAVE_STDLIB_H
  #include <stdlib.h>
#else
  extern char *getenv(const char *);
#endif

/* Maximum length of a variable name (not including NUL). */
#define ENVSUBST_MAX_NAME 255

static int is_var_start(unsigned char c)
{
  return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_');
}

static int is_var_cont(unsigned char c)
{
  return ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_');
}

/*
 * Append n bytes from s to *dst, keeping *dst < end.
 * Returns false if the string does not fit.
 */
static bool append_buf(const char *s, size_t n, char **dst, char *end)
{
  if ((size_t)(end - *dst) < n) {
    return false;
  }
  memcpy(*dst, s, n);
  *dst += n;
  return true;
}

bool envsubst(const char *src, char *out, size_t out_size)
{
  const char *p;
  char *dst;
  char *end;

  if (!src || !out || out_size == 0) {
    return false;
  }

  p = src;
  dst = out;
  /* end is the slot reserved for the NUL terminator; never written as data. */
  end = out + out_size - 1;

  while (*p) {
    if (*p != '$') {
      if (dst >= end) {
        return false;
      }
      *dst++ = *p++;
      continue;
    }

    /* *p == '$': advance past it and inspect the next character. */
    p++;

    if (*p == '{') {
      /* ${VAR} or ${VAR:-default} form */
      const char *inner = p + 1;
      const char *brace_end;
      const char *sep;
      const char *q;
      const char *name_start;
      const char *default_str;
      size_t name_len;
      size_t default_len;
      bool valid;
      size_t i;
      char name_buf[ENVSUBST_MAX_NAME + 1];
      const char *val;

      brace_end = strchr(inner, '}');
      if (!brace_end) {
        /* Unterminated '${': emit '$' as literal; re-process from '{'. */
        if (dst >= end) {
          return false;
        }
        *dst++ = '$';
        continue;
      }

      /* Locate the first ':-' within the braces. */
      sep = NULL;
      for (q = inner; q + 1 < brace_end; q++) {
        if (q[0] == ':' && q[1] == '-') {
          sep = q;
          break;
        }
      }

      name_start  = inner;
      name_len    = sep ? (size_t)(sep - inner) : (size_t)(brace_end - inner);
      default_str = sep ? sep + 2 : NULL;
      default_len = sep ? (size_t)(brace_end - default_str) : 0;

      /* Validate the variable name. */
      valid = (name_len > 0) && (name_len <= ENVSUBST_MAX_NAME)
           && is_var_start((unsigned char)name_start[0]);
      for (i = 1; i < name_len && valid; i++) {
        if (!is_var_cont((unsigned char)name_start[i])) {
          valid = false;
        }
      }

      if (!valid) {
        /* Not a valid reference: emit '$' as literal; re-process from '{'. */
        if (dst >= end) {
          return false;
        }
        *dst++ = '$';
        continue;
      }

      memcpy(name_buf, name_start, name_len);
      name_buf[name_len] = '\0';

      val = getenv(name_buf);

      if (sep && (val == NULL || val[0] == '\0')) {
        if (!append_buf(default_str, default_len, &dst, end)) {
          return false;
        }
      }
      else if (val) {
        if (!append_buf(val, strlen(val), &dst, end)) {
          return false;
        }
      }
      /* else: unset with no default -- expand to empty string. */

      p = brace_end + 1;
    }
    else if (is_var_start((unsigned char)*p)) {
      /* $VAR form */
      const char *name_start = p;
      size_t name_len;
      char name_buf[ENVSUBST_MAX_NAME + 1];
      const char *val;

      while (*p && is_var_cont((unsigned char)*p)) {
        p++;
      }
      name_len = (size_t)(p - name_start);

      if (name_len > ENVSUBST_MAX_NAME) {
        /* Name too long: emit '$' as literal; re-process from name start. */
        if (dst >= end) {
          return false;
        }
        *dst++ = '$';
        p = name_start;
        continue;
      }

      memcpy(name_buf, name_start, name_len);
      name_buf[name_len] = '\0';

      val = getenv(name_buf);
      if (val) {
        if (!append_buf(val, strlen(val), &dst, end)) {
          return false;
        }
      }
      /* else: unset -- expand to empty string. */
    }
    else {
      /* '$' not followed by '{' or a valid name start: emit '$' as literal. */
      if (dst >= end) {
        return false;
      }
      *dst++ = '$';
      /* Leave *p unchanged; it will be processed in the next iteration. */
    }
  }

  *dst = '\0';
  return true;
}
