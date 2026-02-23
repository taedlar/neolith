#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "hash.h"
#include "array.h"
#include "class.h"
#include "object.h"
#include "otable.h"
#include "mapping.h"
#include "program.h"
#include "src/comm.h"
#include "rc.h"
#include "src/frame.h"
#include "src/simul_efun.h"
#include "lpc/include/origin.h"
#include "lpc/include/runtime_config.h"
#include "efuns/call_out.h"
#include "efuns/file_utils.h"
#include "socket/socket_efuns.h"

#include <sys/stat.h>

#define too_deep_save_error() \
    error("Mappings and/or arrays nested too deep (%d) for save_object\n", MAX_SAVE_SVALUE_DEPTH);

size_t tot_alloc_object = 0, tot_alloc_object_size = 0;

char *save_mapping (mapping_t * m);
static int restore_array (char **str, svalue_t *);
static int restore_class (char **str, svalue_t *);
int restore_hash_string (char **str, svalue_t *);

int valid_hide (object_t * obj) {
  svalue_t *ret;

  if (!obj)
    {
      return 0;
    }
  push_object (obj);
  ret = apply_master_ob (APPLY_VALID_HIDE, 1);
  return (!IS_ZERO (ret));
}


int save_svalue_depth = 0;
int save_max_depth;
int *save_svalue_sizes = 0;

/**
 * Calculate the size needed to save an svalue_t.
 */
size_t svalue_save_size (const svalue_t * v) {
  switch (v->type)
    {
    case T_STRING:
      {
        register char *cp = v->u.string;
        char c;
        size_t size = 0;

        while ((c = *cp++))
          {
            if (c == '\\' || c == '"') /* need to escape these characters */
              size++;
            size++;
          }
        return 3 + size; /* 2 for quotes, 1 for comma/colon delimiter */
      }

    case T_ARRAY:
      {
        svalue_t *sv = v->u.arr->item;
        int i = v->u.arr->size;
        size_t size = 0;

        if (++save_svalue_depth > MAX_SAVE_SVALUE_DEPTH)
          {
            too_deep_save_error ();
          }
        while (i--)
          size += svalue_save_size (sv++);
        save_svalue_depth--;
        return size + 5; /* 5 for ({ and }), 1 for comma delimiter */
      }

    case T_CLASS:
      {
        svalue_t *sv = v->u.arr->item;
        int i = v->u.arr->size;
        size_t size = 0;

        if (++save_svalue_depth > MAX_SAVE_SVALUE_DEPTH)
          {
            too_deep_save_error ();
          }
        while (i--)
          size += svalue_save_size (sv++);
        save_svalue_depth--;
        return size + 5; /* 5 for (/ and /), 1 for comma delimiter */
      }

    case T_MAPPING:
      {
        mapping_node_t **a = v->u.map->table, *elt;
        int j = v->u.map->table_size;
        size_t size = 0;

        if (++save_svalue_depth > MAX_SAVE_SVALUE_DEPTH)
          {
            too_deep_save_error ();
          }
        do
          {
            for (elt = a[j]; elt; elt = elt->next)
              {
                size += svalue_save_size (elt->values) + svalue_save_size (elt->values + 1);
              }
          }
        while (j--);
        save_svalue_depth--;
        return size + 5; /* 5 for ([ and ]), 1 for comma delimiter */
      }

    case T_NUMBER:
      {
        int64_t res = v->u.number;
        size_t len;
        len = res < 0 ? (res = (-res), 1) : 0; /* +1 for sign if negative, count digits with positive value */
        while (res > 9)
          {
            res /= 10;
            len++;
          }
        return len + 2; /* 1 for least significant digit, 1 for comma/colon */
      }

    case T_REAL:
      {
        char buf[256];
        sprintf (buf, "%g", v->u.real);
        return strlen (buf) + 1; /* 1 for comma/colon */
      }

    default:
      return 2; /* zero, plus comma/colon delimiter */
    }
}

void save_svalue (svalue_t * v, char **buf) {
  switch (v->type)
    {
    case T_STRING:
      {
        register char *cp = *buf, *str = v->u.string;
        char c;

        *cp++ = '"';
        while ((c = *str++))
          {
            if (c == '"' || c == '\\')
              {
                *cp++ = '\\';
                *cp++ = c;
              }
            else
              *cp++ = (c == '\n') ? '\r' : c;
          }

        *cp++ = '"';
        *(*buf = cp) = '\0';
        return;
      }

    case T_ARRAY:
      {
        int i = v->u.arr->size;
        svalue_t *sv = v->u.arr->item;

        *(*buf)++ = '(';
        *(*buf)++ = '{';
        while (i--)
          {
            save_svalue (sv++, buf);
            *(*buf)++ = ',';
          }
        *(*buf)++ = '}';
        *(*buf)++ = ')';
        *(*buf) = '\0';
        return;
      }

    case T_CLASS:
      {
        int i = v->u.arr->size;
        svalue_t *sv = v->u.arr->item;

        *(*buf)++ = '(';
        *(*buf)++ = '/';	/* Why yes, this *is* a kludge! */
        while (i--)
          {
            save_svalue (sv++, buf);
            *(*buf)++ = ',';
          }
        *(*buf)++ = '/';
        *(*buf)++ = ')';
        *(*buf) = '\0';
        return;
      }

    case T_NUMBER:
      {
        int64_t res = v->u.number, fact;
        size_t len = 1; /* least significant digit */
        int neg = 0;
        register char *cp;

        if (res < 0)
          {
            len++; /* +1 for sign if negative */
            neg = 1;
            res = (-res);
          }
        fact = res;
        while (fact > 9)
          {
            fact /= 10;
            len++; /* count digits */
          }
        *(cp = (*buf += len)) = '\0';
        do
          {
            *--cp = res % 10 + '0';
            res /= 10;
          }
        while (res);
        if (neg)
          *(cp - 1) = '-';
        return;
      }

    case T_REAL:
      {
        sprintf (*buf, "%g", v->u.real);
        (*buf) += strlen (*buf);
        return;
      }

    case T_MAPPING:
      {
        int j = v->u.map->table_size;
        mapping_node_t **a = v->u.map->table, *elt;

        *(*buf)++ = '(';
        *(*buf)++ = '[';
        do
          {
            for (elt = a[j]; elt; elt = elt->next)
              {
                save_svalue (elt->values, buf);
                *(*buf)++ = ':';
                save_svalue (elt->values + 1, buf);
                *(*buf)++ = ',';
              }
          }
        while (j--);

        *(*buf)++ = ']';
        *(*buf)++ = ')';
        *(*buf) = '\0';
        return;
      }
    }
}

static int restore_internal_size (char **str, int is_mapping, int depth) {
  register char *cp = *str;
  int size = 0;
  char c, delim, index = 0;

  delim = is_mapping ? ':' : ',';
  while ((c = *cp++))
    {
      switch (c)
        {
        case '"':
          {
            while ((c = *cp++) != '"')
              if ((c == '\0') || (c == '\\' && !*cp++))
                {
                  return 0;
                }
            if (*cp++ != delim)
              return 0;
            size++;
            break;
          }

        case '(':
          {
            if (*cp == '{')
              {
                *str = ++cp;
                if (!restore_internal_size (str, 0, save_svalue_depth++))
                  {
                    return 0;
                  }
              }
            else if (*cp == '[')
              {
                *str = ++cp;
                if (!restore_internal_size (str, 1, save_svalue_depth++))
                  {
                    return 0;
                  }
              }
            else if (*cp == '/')
              {
                *str = ++cp;
                if (!restore_internal_size (str, 0, save_svalue_depth++))
                  return 0;
              }
            else
              {
                return 0;
              }

            if (*(cp = *str) != delim)
              {
                return 0;
              }
            cp++;
            size++;
            break;
          }

        case ']':
          {
            if (*cp++ == ')' && is_mapping)
              {
                *str = cp;
                if (!save_svalue_sizes)
                  {
                    save_max_depth = 128;
                    while (save_max_depth <= depth)
                      save_max_depth <<= 1;
                    save_svalue_sizes = CALLOCATE (save_max_depth, int, TAG_TEMPORARY, "restore_internal_size");
                  }
                else if (depth >= save_max_depth)
                  {
                    while ((save_max_depth <<= 1) <= depth);
                    save_svalue_sizes = RESIZE (save_svalue_sizes, save_max_depth, int, TAG_TEMPORARY, "restore_internal_size");
                  }
                save_svalue_sizes[depth] = size;
                return 1;
              }
            else
              {
                return 0;
              }
          }

        case '/':
        case '}':
          {
            if (*cp++ == ')' && !is_mapping)
              {
                *str = cp;
                if (!save_svalue_sizes)
                  {
                    save_max_depth = 128;
                    while (save_max_depth <= depth)
                      save_max_depth <<= 1;
                    save_svalue_sizes = CALLOCATE (save_max_depth, int, TAG_TEMPORARY,
                                       "restore_internal_size");
                  }
                else if (depth >= save_max_depth)
                  {
                    while ((save_max_depth <<= 1) <= depth);
                    save_svalue_sizes = RESIZE (save_svalue_sizes, save_max_depth, int, TAG_TEMPORARY, "restore_internal_size");
                  }
                save_svalue_sizes[depth] = size;
                return 1;
              }
            else
              {
                return 0;
              }
          }

        case ':':
        case ',':
          {
            if (c != delim)
              return 0;
            size++;
            break;
          }

        default:
          {
            if (!(cp = strchr (cp, delim)))
              return 0;
            cp++;
            size++;
          }
        }
      if (is_mapping)
        delim = (index ^= 1) ? ',' : ':';
    }
  return 0;
}

static int restore_size (char **str, int is_mapping) {
  register char *cp = *str;
  int size = 0, mb_span;
  char c, delim, index = 0;

  delim = is_mapping ? ':' : ',';

  while ((c = *cp))
    {
      mb_span = mblen (cp, MB_CUR_MAX);
      if (mb_span < 0)
                    return -1;
      cp += mb_span; /* don't check in the middle of a multibyte character */
      switch (c)
                    {
                      case '"':
                        {
                    char* start = cp - 1;
                    while ((c = *cp) != '"')
                      {
                              mb_span = mblen (cp, MB_CUR_MAX);
                              if (mb_span < 0)
                                return -1;
                              cp += mb_span; /* don't check backslash in the middle of a multibyte character */
                              if ((c == '\0') || (c == '\\' && !*cp++))
                                return 0;
                      }
                    cp++;

                    if (*cp++ != delim)
                      {
                              debug_error ("corrupted multibyte string: %s", start);
                              return -1;
                      }
                    size++;
                    break;
                  }

                case '(':
                  {
                    if (*cp == '{')
                      {
                              *str = ++cp;
                              if (!restore_internal_size (str, 0, save_svalue_depth++))
                                return -1;
                      }
                    else if (*cp == '[')
                      {
                              *str = ++cp;
                              if (!restore_internal_size (str, 1, save_svalue_depth++))
                                return -1;
                      }
                    else if (*cp == '/')
                      {
                              *str = ++cp;
                              if (!restore_internal_size (str, 0, save_svalue_depth++))
                                return -1;
                      }
                    else
                      {
                              return -1;
                }

                    if (*(cp = *str) != delim)
                      {
                              return -1;
                      }
                    cp++;
                    size++;
                    break;
                  }

                case ']':
                  {
                    save_svalue_depth = 0;
                    if (*cp++ == ')' && is_mapping)
                      {
                              *str = cp;
                              return size;
                      }
                    else
                      {
                              return -1;
                      }
                  }

                case '/':
                case '}':
                  {
                    save_svalue_depth = 0;
                    if (*cp++ == ')' && !is_mapping)
                      {
                              *str = cp;
                              return size;
                      }
                    else
                      {
                              return -1;
                      }
                  }

                case ':':
                case ',':
                  {
                    if (c != delim)
                      return -1;
                    size++;
                    break;
                  }

                default:
                  {
                    if (!(cp = strchr (cp, delim)))
                      {
                              return -1;
                      }
                    cp++;
                    size++;
                  }
              }
      if (is_mapping)
              delim = (index ^= 1) ? ',' : ':';
    }
  return -1;
}

static int restore_interior_string (char **val, svalue_t * sv) {
  register char *cp = *val;
  char *start = cp;
  char c;
  size_t len;

  while ((c = *cp++) != '"')
    {
      switch (c)
        {
        case '\r':
          {
            *(cp - 1) = '\n';
            break;
          }

        case '\\':
          {
            char *newp = cp - 1;

            if ((*newp++ = *cp++))
              {
                while ((c = *cp++) != '"')
                  {
                    if (c == '\\')
                      {
                        if (!(*newp++ = *cp++))
                          return ROB_STRING_ERROR;
                      }
                    else
                      {
                        if (c == '\r')
                          *newp++ = '\n';
                        else
                          *newp++ = c;
                      }
                  }
                if (c == '\0')
                  return ROB_STRING_ERROR;
                *newp = '\0';
                *val = cp;
                sv->u.string = new_string (len = (newp - start),
                                           "restore_string");
                strcpy (sv->u.string, start);
                sv->type = T_STRING;
                sv->subtype = STRING_MALLOC;
                return 0;
              }
            else
              return ROB_STRING_ERROR;
          }

        case '\0':
          {
            return ROB_STRING_ERROR;
          }

        }
    }

  *val = cp;
  *--cp = '\0';
  len = (size_t)(cp - start);
  sv->u.string = new_string (len, "restore_string");
  strcpy (sv->u.string, start);
  sv->type = T_STRING;
  sv->subtype = STRING_MALLOC;
  return 0;
}

static int parse_numeric (char **cpp, char c, svalue_t * dest) {
  char *cp = *cpp;
  int res, neg;

  if (c == '-')
    {
      neg = 1;
      res = 0;
      c = *cp++;
      if (!isdigit (c))
        return 0;
    }
  else
    neg = 0;
  res = c - '0';

  while ((c = *cp++) && isdigit (c))
    {
      res *= 10;
      res += c - '0';
    }
  if (c == '.')
    {
      double f1 = 0.0, f2 = 10.0;

      c = *cp++;
      if (!isdigit (c))
        return 0;

      do
        {
          f1 += (c - '0') / f2;
          f2 *= 10;
        }
      while ((c = *cp++) && isdigit (c));

      f1 += res;
      if (c == 'e')
        {
          int expo = 0;

          if ((c = *cp++) == '+')
            {
              while ((c = *cp++) && isdigit (c))
                {
                  expo *= 10;
                  expo += (c - '0');
                }
              f1 *= pow (10.0, expo);
            }
          else if (c == '-')
            {
              while ((c = *cp++) && isdigit (c))
                {
                  expo *= 10;
                  expo += (c - '0');
                }
              f1 *= pow (10.0, -expo);
            }
          else
            return 0;
        }

      dest->type = T_REAL;
      dest->u.real = (neg ? -f1 : f1);
      *cpp = cp;
      return 1;
    }
  else if (c == 'e')
    {
      int expo = 0;
      double f1;

      if ((c = *cp++) == '+')
        {
          while ((c = *cp++) && isdigit (c))
            {
              expo *= 10;
              expo += (c - '0');
            }
          f1 = res * pow (10.0, expo);
        }
      else if (c == '-')
        {
          while ((c = *cp++) && isdigit (c))
            {
              expo *= 10;
              expo += (c - '0');
            }
          f1 = res * pow (10.0, -expo);
        }
      else
        return 0;

      dest->type = T_REAL;
      dest->u.real = (neg ? -f1 : f1);
      *cpp = cp;
      return 1;
    }
  else
    {
      dest->type = T_NUMBER;
      dest->u.number = (neg ? -res : res);
      *cpp = cp;
      return 1;
    }
}

static void add_map_stats (mapping_t * m, int count) {
  total_mapping_nodes += count;
  total_mapping_size += count * sizeof (mapping_node_t);
  m->count = count;
}

int growMap (mapping_t *);

static int restore_mapping (char **str, svalue_t * sv) {
  int size, i, mask, oi, count = 0;
  char c;
  mapping_t *m;
  svalue_t key, value;
  mapping_node_t **a, *elt, *elt2;
  char *cp = *str;
  int err;

  if (save_svalue_depth)
    size = save_svalue_sizes[save_svalue_depth - 1];
  else if ((size = restore_size (str, 1)) < 0)
    {
      debug_error ("corrupted");
      return 0;
    }

  if (!size)
    {
      *str += 2;
      sv->u.map = allocate_mapping (0);
      sv->type = T_MAPPING;
      return 0;
    }
  m = allocate_mapping (size >> 1);	/* have to clean up after this or */
  a = m->table;			/* we'll leak */
  mask = m->table_size;

  while (1)
    {
      switch (c = *cp++)
        {
        case '"':
          {
            *str = cp;
            if ((err = restore_hash_string (str, &key)))
              goto key_error;
            cp = *str;
            cp++;
            break;
          }

        case '(':
          {
            save_svalue_depth++;
            if (*cp == '[')
              {
                *str = ++cp;
                if ((err = restore_mapping (str, &key)))
                  goto key_error;
              }
            else if (*cp == '{')
              {
                *str = ++cp;
                if ((err = restore_array (str, &key)))
                  goto key_error;
              }
            else if (*cp == '/')
              {
                *str = ++cp;
                if ((err = restore_class (str, &key)))
                  goto key_error;
              }
            else
              goto generic_key_error;
            cp = *str;
            cp++;
            break;
          }

        case ':':
          {
            key.u.number = 0;
            key.type = T_NUMBER;
            break;
          }

        case ']':
          *str = ++cp;
          add_map_stats (m, count);
          sv->type = T_MAPPING;
          sv->u.map = m;
          return 0;

        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          if (!parse_numeric (&cp, c, &key))
            goto key_numeral_error;
          break;

        default:
          goto generic_key_error;
        }

      /* At this point, key is a valid, referenced svalue and we're
         responsible for it */

      switch (c = *cp++)
        {
        case '"':
          {
            *str = cp;
            if ((err = restore_interior_string (str, &value)))
              goto value_error;
            cp = *str;
            cp++;
            break;
          }

        case '(':
          {
            save_svalue_depth++;
            if (*cp == '[')
              {
                *str = ++cp;
                if ((err = restore_mapping (str, &value)))
                  goto value_error;
              }
            else if (*cp == '{')
              {
                *str = ++cp;
                if ((err = restore_array (str, &value)))
                  goto value_error;
              }
            else if (*cp == '/')
              {
                *str = ++cp;
                if ((err = restore_class (str, &value)))
                  goto value_error;
              }
            else
              goto generic_value_error;
            cp = *str;
            cp++;
            break;
          }

        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          if (!parse_numeric (&cp, c, &value))
            goto value_numeral_error;
          break;

        case ',':
          {
            value.u.number = 0;
            value.type = T_NUMBER;
            break;
          }

        default:
          goto generic_value_error;
        }

      /* both key and value are valid, referenced svalues */

      oi = (int)MAP_POINTER_HASH (key.u.number);
      i = oi & mask;
      if ((elt2 = elt = a[i]))
        {
          do
            {
              /* This should never happen, but don't bail on it */
              if (msameval (&key, elt->values))
                {
                  free_svalue (&key, "restore_mapping: duplicate key");
                  free_svalue (elt->values + 1,
                               "restore_mapping: replaced value");
                  *(elt->values + 1) = value;
                  break;
                }
            }
          while ((elt = elt->next));
          if (elt)
            continue;
        }
      else if (!(--m->unfilled))
        {
          if (growMap (m))
            {
              a = m->table;
              if (oi & ++mask)
                elt2 = a[i |= mask];
              mask <<= 1;
              mask--;
            }
          else
            {
              add_map_stats (m, count);
              free_mapping (m);
              free_svalue (&key, "restore_mapping: out of memory");
              free_svalue (&value, "restore_mapping: out of memory");
              error ("Out of memory\n");
            }
        }

      if (++count > CONFIG_INT (__MAX_MAPPING_SIZE__))
        {
          add_map_stats (m, count - 1);
          free_mapping (m);
          free_svalue (&key, "restore_mapping: mapping too large");
          free_svalue (&value, "restore_mapping: mapping too large");
          mapping_too_large ();
        }

      elt = new_map_node ();
      *elt->values = key;
      *(elt->values + 1) = value;
      (a[i] = elt)->next = elt2;
    }

  /* something went wrong */
value_numeral_error:
  free_svalue (&key, "restore_mapping: numeral value error");
key_numeral_error:
  add_map_stats (m, count);
  free_mapping (m);
  return ROB_NUMERAL_ERROR;
generic_value_error:
  free_svalue (&key, "restore_mapping: generic value error");
generic_key_error:
  add_map_stats (m, count);
  free_mapping (m);
  return ROB_MAPPING_ERROR;
value_error:
  free_svalue (&key, "restore_mapping: value error");
key_error:
  add_map_stats (m, count);
  free_mapping (m);
  return err;
}


static int restore_class (char **str, svalue_t * ret) {
  int size;
  char c;
  array_t *v;
  svalue_t *sv;
  char *cp = *str;
  int err;

  if (save_svalue_depth)
    size = save_svalue_sizes[save_svalue_depth - 1];
  else if ((size = restore_size (str, 0)) < 0)
    return ROB_CLASS_ERROR;

  v = allocate_class_by_size (size);	/* after this point we have to clean up
                                           or we'll leak */
  sv = v->item;

  while (size--)
    {
      switch (c = *cp++)
        {
        case '"':
          *str = cp;
          if ((err = restore_interior_string (str, sv)))
            goto generic_error;
          cp = *str;
          cp++;
          sv++;
          break;

        case ',':
          sv++;
          break;

        case '(':
          {
            save_svalue_depth++;
            if (*cp == '[')
              {
                *str = ++cp;
                if ((err = restore_mapping (str, sv)))
                  goto error;
              }
            else if (*cp == '{')
              {
                *str = ++cp;
                if ((err = restore_array (str, sv)))
                  goto error;
              }
            else if (*cp == '/')
              {
                *str = ++cp;
                if ((err = restore_class (str, sv)))
                  goto error;
              }
            else
              goto generic_error;
            sv++;
            cp = *str;
            cp++;
            break;
          }

        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          if (parse_numeric (&cp, c, sv))
            sv++;
          else
            goto numeral_error;
          break;

        default:
          goto generic_error;
        }
    }

  cp += 2;
  *str = cp;
  ret->u.arr = v;
  ret->type = T_CLASS;
  return 0;
  /* something went wrong */
numeral_error:
  err = ROB_NUMERAL_ERROR;
  goto error;
generic_error:
  err = ROB_CLASS_ERROR;
error:
  free_class (v);
  return err;
}

static int restore_array (char **str, svalue_t * ret) {
  int size;
  char c;
  array_t *v;
  svalue_t *sv;
  char *cp = *str;
  int err;

  if (save_svalue_depth)
    size = save_svalue_sizes[save_svalue_depth - 1];
  else if ((size = restore_size (str, 0)) < 0)
    return ROB_ARRAY_ERROR;

  v = allocate_array (size);	/* after this point we have to clean up
                                   or we'll leak */
  sv = v->item;

  while (size--)
    {
      switch (c = *cp++)
        {
        case '"':
          *str = cp;
          if ((err = restore_interior_string (str, sv)))
            goto generic_error;
          cp = *str;
          cp++;
          sv++;
          break;

        case ',':
          sv++;
          break;

        case '(':
          {
            save_svalue_depth++;
            if (*cp == '[')
              {
                *str = ++cp;
                if ((err = restore_mapping (str, sv)))
                  goto error;
              }
            else if (*cp == '{')
              {
                *str = ++cp;
                if ((err = restore_array (str, sv)))
                  goto error;
              }
            else if (*cp == '/')
              {
                *str = ++cp;
                if ((err = restore_class (str, sv)))
                  goto error;
              }
            else
              goto generic_error;
            sv++;
            cp = *str;
            cp++;
            break;
          }

        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          if (parse_numeric (&cp, c, sv))
            sv++;
          else
            goto numeral_error;
          break;

        default:
          goto generic_error;
        }
    }

  cp += 2;
  *str = cp;
  ret->u.arr = v;
  ret->type = T_ARRAY;
  return 0;
  /* something went wrong */
numeral_error:
  err = ROB_NUMERAL_ERROR;
  goto error;
generic_error:
  err = ROB_ARRAY_ERROR;
error:
  free_array (v);
  return err;
}

int restore_string (char *val, svalue_t * sv) {
  register char *cp = val;
  char *start = cp;
  char c;
  size_t len;

  while ((c = *cp++) != '"')
    {
      switch (c)
        {
        case '\r':
          {
            *(cp - 1) = '\n';
            break;
          }

        case '\\':
          {
            char *newp = cp - 1;

            if ((*newp++ = *cp++))
              {
                while ((c = *cp++) != '"')
                  {
                    if (c == '\\')
                      {
                        if (!(*newp++ = *cp++))
                          return ROB_STRING_ERROR;
                      }
                    else
                      {
                        if (c == '\r')
                          *newp++ = '\n';
                        else
                          *newp++ = c;
                      }
                  }
                if ((c == '\0') || (*cp != '\0'))
                  return ROB_STRING_ERROR;
                *newp = '\0';
                sv->u.string = new_string (newp - start, "restore_string");
                strcpy (sv->u.string, start);
                sv->type = T_STRING;
                sv->subtype = STRING_MALLOC;
                return 0;
              }
            else
              return ROB_STRING_ERROR;
          }

        case '\0':
          {
            return ROB_STRING_ERROR;
          }

        }
    }

  if (*cp--)
    return ROB_STRING_ERROR;
  *cp = '\0';
  len = (size_t)(cp - start);
  sv->u.string = new_string (len, "restore_string");
  strcpy (sv->u.string, start);
  sv->type = T_STRING;
  sv->subtype = STRING_MALLOC;
  return 0;
}

/* for this case, the variable in question has been set to zero already,
   and we don't have to worry about preserving it */
int restore_svalue (char *cp, svalue_t * v) {
  int ret;
  char c;

  switch (c = *cp++)
    {
    case '"':
      return restore_string (cp, v);
    case '(':
      if (*cp == '{')
        {
          cp++;
          ret = restore_array (&cp, v);
        }
      else if (*cp == '[')
        {
          cp++;
          ret = restore_mapping (&cp, v);
        }
      else if (*cp++ == '/')
        {
          ret = restore_class (&cp, v);
        }
      else
        ret = ROB_GENERAL_ERROR;

      if (save_svalue_depth)
        {
          save_svalue_depth = save_max_depth = 0;
          if (save_svalue_sizes)
            FREE ((char *) save_svalue_sizes);
          save_svalue_sizes = (int *) 0;
        }
      return ret;

    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      if (!parse_numeric (&cp, c, v))
        return ROB_NUMERAL_ERROR;
      break;

    default:
      v->type = T_NUMBER;
      v->u.number = 0;
    }

  return 0;
}

/* for this case, we're being careful and want to leave the value alone on
   an error */
int safe_restore_svalue (char *cp, svalue_t * v) {
  int ret;
  svalue_t val;
  char c;

  val.type = T_NUMBER;
  switch (c = *cp++)
    {
    case '"':
      if ((ret = restore_string (cp, &val)))
        return ret;
      break;
    case '(':
      {
        if (*cp == '{')
          {
            cp++;
            ret = restore_array (&cp, &val);
          }
        else if (*cp == '[')
          {
            cp++;
            ret = restore_mapping (&cp, &val);
          }
        else if (*cp++ == '/')
          {
            ret = restore_class (&cp, &val);
          }
        else
          return ROB_GENERAL_ERROR;

        if (save_svalue_depth)
          {
            save_svalue_depth = save_max_depth = 0;
            if (save_svalue_sizes)
              FREE ((char *) save_svalue_sizes);
            save_svalue_sizes = (int *) 0;
          }
        if (ret)
          return ret;
        break;
      }

    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      if (!parse_numeric (&cp, c, &val))
        return ROB_NUMERAL_ERROR;
      break;

    default:
      val.type = T_NUMBER;
      val.u.number = 0;
    }
  free_svalue (v, "safe_restore_svalue");
  *v = val;
  return 0;
}

static int fgv_recurse (program_t * prog, int *idx, char *name, unsigned short *type) {
  int i;
  for (i = 0; i < prog->num_inherited; i++)
    {
      if (fgv_recurse (prog->inherit[i].prog, idx, name, type))
        {
          *type |= prog->inherit[i].type_mod;
          return 1;
        }
    }
  for (i = 0; i < prog->num_variables_defined; i++)
    {
      if (prog->variable_table[i] == name)
        {
          *idx += i;
          *type = prog->variable_types[i];
          return 1;
        }
    }
  *idx += prog->num_variables_defined;
  return 0;
}

int find_global_variable (program_t * prog, char *name, unsigned short *type) {
  int idx = 0;
  char *str = findstring (name);

  if (str && fgv_recurse (prog, &idx, str, type))
    {
      if (*type & NAME_PUBLIC)
        *type &= ~NAME_PRIVATE;
      return idx;
    }
  return -1;
}

void restore_object_from_buff (object_t * ob, char *theBuff, int noclear) {
  char *buff, *nextBuff, *tmp, *space;
  char var[100];
  int idx;
  svalue_t *sv = ob->variables;
  int rc;
  unsigned short t;

  nextBuff = theBuff;
  while ((buff = nextBuff) && *buff)
    {
      svalue_t *v;

      if ((tmp = strchr (buff, '\n')))
        {
          *tmp = '\0';
          nextBuff = tmp + 1;
        }
      else
        {
          nextBuff = 0;
        }
      if (buff[0] == '#')	/* ignore 'comments' in savefiles */
        continue;
      space = strchr (buff, ' ');
      if (!space || ((space - buff) >= (int)sizeof (var)))
        {
          FREE (theBuff);
          error ("restore_object(): Illegal file format.\n");
        }
      (void) strncpy (var, buff, space - buff);
      var[space - buff] = '\0';
      idx = find_global_variable (current_object->prog, var, &t);
      if (idx == -1 || t & NAME_STATIC)
        continue;

      v = &sv[idx];
      if (noclear)
        rc = safe_restore_svalue (space + 1, v);
      else
        rc = restore_svalue (space + 1, v);
      if (rc & ROB_ERROR)
        {
          FREE (theBuff);

          if (rc & ROB_GENERAL_ERROR)
            error ("restore_object(): Illegal general format while restoring %s.\n", var);
          else if (rc & ROB_NUMERAL_ERROR)
            error ("restore_object(): Illegal numeric format while restoring %s.\n", var);
          else if (rc & ROB_ARRAY_ERROR)
            error ("restore_object(): Illegal array format while restoring %s.\n", var);
          else if (rc & ROB_MAPPING_ERROR)
            error ("restore_object(): Illegal mapping format while restoring %s.\n", var);
          else if (rc & ROB_STRING_ERROR)
            error ("restore_object(): Illegal string format while restoring %s.\n", var);
          else if (rc & ROB_CLASS_ERROR)
            error ("restore_object(): Illegal class format while restoring %s.\n", var);
        }
    }
}

/*
 * Save an object to a file.
 * The routine checks with the function "valid_write()" in /obj/master.c
 * to assertain that the write is legal.
 * If 'save_zeros' is set, 0 valued variables will be saved
 */
static int save_object_recurse (program_t * prog, svalue_t ** svp, int type, int save_zeros, FILE * f) {
  int i;
  size_t theSize;
  char *new_str, *p;

  for (i = 0; i < prog->num_inherited; i++)
    {
      if (!save_object_recurse (prog->inherit[i].prog, svp,
                                prog->inherit[i].type_mod | type,
                                save_zeros, f))
        return 0;
    }
  if (type & NAME_STATIC)
    {
      (*svp) += prog->num_variables_defined;
      return 1;
    }
  for (i = 0; i < prog->num_variables_defined; i++)
    {
      if (prog->variable_types[i] & NAME_STATIC)
        {
          (*svp)++;
          continue;
        }
      save_svalue_depth = 0;
      theSize = svalue_save_size (*svp);
      new_str = (char *) DXALLOC (theSize, TAG_TEMPORARY, "save_object: 2");
      *new_str = '\0';
      p = new_str;
      save_svalue ((*svp)++, &p);
      /* FIXME: shouldn't use fprintf() */
      if (save_zeros || new_str[0] != '0' || new_str[1] != 0)	/* Armidale */
        if (fprintf (f, "%s %s\n", prog->variable_table[i], new_str) < 0)
          {
            debug_perror ("save_object: fprintf", 0);
            return 0;
          }
      FREE (new_str);
    }
  return 1;
}

static size_t sel = (size_t)-1; /* save extension length */

/**
 * @brief Save an object to a file.
 * @returns 1 on success, 0 on failure.
 */
int save_object (object_t * ob, const char *file, int save_zeros) {

  char *name;
  static char tmp_name[256];
  size_t len;
  FILE *f;
  int success;
  svalue_t *v;

  if (ob->flags & O_DESTRUCTED)
    return 0;

  len = strlen (file);
  if (file[len - 2] == '.' && file[len - 1] == 'c')
    len -= 2; /* strip .c */

  if (sel == (size_t)-1)
    sel = strlen (SAVE_EXTENSION);
  if (strcmp (file + len - sel, SAVE_EXTENSION) == 0)
    len -= sel; /* strip SAVE_EXTENSION if already present */

  name = new_string (len + strlen (SAVE_EXTENSION), "save_object");
  strcpy (name, file);
  strcpy (name + len, SAVE_EXTENSION);
  push_malloced_string (name);	/* errors */

  file = check_valid_path (name, ob, "save_object", 1);
  if (!file)
    {
      /* error ("Denied write permission in save_object().\n"); */
      free_string_svalue (sp--);
      return 0;
    }

  /*
   * Write the save-files to different directories, just in case
   * they are on different file systems.
   */
  snprintf (tmp_name, sizeof(tmp_name), "%.250s.tmp", file);
  tmp_name[sizeof(tmp_name) - 1] = '\0';

  opt_trace (TT_EVAL|1, "creating tmp file: %s", tmp_name);
  f = fopen (tmp_name, "w");
  if (!f)
    {
      debug_perror ("fopen()", tmp_name);
      free_string_svalue (sp--);
      return 0;  
    }

  if (fprintf (f, "#/%s\n", ob->prog->name) < 0)
    {
      debug_perror ("Could not write save_object() header", tmp_name);
      free_string_svalue (sp--);
      return 0;
    }

  v = ob->variables;
  success = save_object_recurse (ob->prog, &v, 0, save_zeros, f);

  if (fclose (f) < 0)
    {
      debug_perror ("fclose()", tmp_name);
      success = 0;
    }

  if (!success)
    {
      debug_message ("Failed to completely save file. Disk could be full.\n");
      unlink (tmp_name);
    }
  else
    {
#ifdef WIN32
      /* Need to erase it to write over it. */
      unlink (file);
#endif
      opt_trace (TT_EVAL|1, "renaming %s to %s", tmp_name, file);
      if (rename (tmp_name, file) < 0)
        {
          debug_perror ("rename()", file);
          debug_message ("save_obecjt(): Failed to rename /%s to /%s", tmp_name, file);
          unlink (tmp_name);
          success = 0;
        }
    }

  free_string_svalue (sp--);
  return success ? 1 : 0;
}


/*
 * return a string representing an svalue in the form that save_object()
 * would write it.
 */
char* save_variable (svalue_t * var) {
  size_t theSize;
  char *new_str, *p;

  save_svalue_depth = 0;
  theSize = svalue_save_size (var);
  new_str = new_string (theSize - 1, "save_variable");
  *new_str = '\0';
  p = new_str;
  save_svalue (var, &p);
  return new_str;
}

static void cns_just_count (int *idx, program_t * prog) {
  int i;

  for (i = 0; i < prog->num_inherited; i++)
    cns_just_count (idx, prog->inherit[i].prog);
  *idx += prog->num_variables_defined;
}

static void cns_recurse (object_t * ob, int *idx, program_t * prog) {
  int i;

  for (i = 0; i < prog->num_inherited; i++)
    {
      if (prog->inherit[i].type_mod & NAME_STATIC)
        cns_just_count (idx, prog->inherit[i].prog);
      else
        cns_recurse (ob, idx, prog->inherit[i].prog);
    }
  for (i = 0; i < prog->num_variables_defined; i++)
    {
      if (!(prog->variable_types[i] & NAME_STATIC))
        {
          free_svalue (&ob->variables[*idx + i], "cns_recurse");
          ob->variables[*idx + i] = const0u;
        }
    }
  *idx += prog->num_variables_defined;
}

static void clear_non_statics (object_t * ob) {
  int idx = 0;
  cns_recurse (ob, &idx, ob->prog);
}

int restore_object (object_t * ob, const char *file, int noclear) {

  char *name, *theBuff;
  size_t len;
  int i;
  FILE *f;
  object_t *save = current_object;
  struct stat st;
  size_t n_read;

  if (ob->flags & O_DESTRUCTED)
    return 0;

  len = strlen (file);
  if (file[len - 2] == '.' && file[len - 1] == 'c')
    len -= 2;

  if (sel == (size_t)-1)
    sel = strlen (SAVE_EXTENSION);
  if (strcmp (file + len - sel, SAVE_EXTENSION) == 0)
    len -= sel;

  name = new_string (len + strlen (SAVE_EXTENSION), "restore_object");
  strncpy (name, file, len);
  strcpy (name + len, SAVE_EXTENSION);

  push_malloced_string (name);	/* errors */

  file = check_valid_path (name, ob, "restore_object", 0);
  if (!file)
    {
      free_string_svalue (sp--);
      error ("Denied read permission in restore_object().\n");
    }

  opt_trace (TT_EVAL|1, "restoring object from file: %s", file);
  f = fopen (file, "r");
  if (!f || fstat (fileno (f), &st) == -1)
    {
      if (f)
        (void) fclose (f);
      free_string_svalue (sp--);
      return 0;
    }

  if (!(i = st.st_size))
    {
      (void) fclose (f);
      free_string_svalue (sp--);
      return 0;
    }
  theBuff = DXALLOC (i + 1, TAG_TEMPORARY, "restore_object: 4");
  opt_trace (TT_EVAL|1, "reading %d bytes of saved data", i);
  n_read = fread (theBuff, 1, i, f);
  fclose (f);
#ifdef _WIN32
  /* On Windows, fread may read less than requested even at EOF */
  if (n_read > 0 && feof(f))
#else
  if (n_read != (size_t)i)
#endif
    {
      FREE (theBuff);
      debug_perror ("restore_object()", file);
      free_string_svalue (sp--);
      error ("restore_object(): Read error.\n");
    }
  theBuff[n_read] = '\0';
  current_object = ob;

  /* This next bit added by Armidale@Cyberworld 1/1/93
   * If 'noclear' flag is not set, all non-static variables will be
   * initialized to 0 when restored.
   */
  if (!noclear)
    clear_non_statics (ob);

  restore_object_from_buff (ob, theBuff, noclear);
  current_object = save;

  FREE (theBuff);
  free_string_svalue (sp--);
  return 1;
}

void restore_variable (svalue_t * var, char *str) {
  int rc;

  rc = restore_svalue (str, var);
  if (rc & ROB_ERROR)
    {
      *var = const0;		/* clean up */
      if (rc & ROB_GENERAL_ERROR)
        error ("restore_object(): Illegal general format.\n");
      else if (rc & ROB_NUMERAL_ERROR)
        error ("restore_object(): Illegal numeric format.\n");
      else if (rc & ROB_ARRAY_ERROR)
        error ("restore_object(): Illegal array format.\n");
      else if (rc & ROB_MAPPING_ERROR)
        error ("restore_object(): Illegal mapping format.\n");
      else if (rc & ROB_STRING_ERROR)
        error ("restore_object(): Illegal string format.\n");
    }
}

void tell_npc (object_t * ob, char *str) {
  copy_and_push_string (str);
  apply (APPLY_CATCH_TELL, ob, 1, ORIGIN_DRIVER);
}

/*
 * tell_object: send a message to an object.
 * If it is an interactive object, it will go to his
 * screen. Otherwise, it will go to a local function
 * catch_tell() in that object. This enables communications
 * between users and NPC's, and between other NPC's.
 * If INTERACTIVE_CATCH_TELL is defined then the message always
 * goes to catch_tell unless the target of tell_object is interactive
 * and is the current_object in which case it is written via add_message().
 */
void tell_object (object_t * ob, char *str) {
  if (!ob || (ob->flags & O_DESTRUCTED))
    {
      add_message (0, str);
      return;
    }

  if (ob == master_ob || ob == simul_efun_ob)
    {
      debug_message ("*%s", str);
      return;
    }

  /* if this is on, EVERYTHING goes through catch_tell() */
#ifndef INTERACTIVE_CATCH_TELL
  if (ob->interactive)
    add_message (ob, str);
  else
#endif
    tell_npc (ob, str);
}

static sentence_t *sent_free = 0;
int tot_alloc_sentence;

sentence_t* alloc_sentence () {
  sentence_t *p;

  if (sent_free == 0)
    {
      p = ALLOCATE (sentence_t, TAG_SENTENCE, "alloc_sentence");
      tot_alloc_sentence++;
    }
  else
    {
      p = sent_free;
      sent_free = sent_free->next;
    }
  p->verb = 0;
  p->function.s = 0;
  p->ob = 0;
  p->flags = 0;
  p->next = 0;
  p->args = NULL;  /* initialize carryover args */
  return p;
}

void free_sentence (sentence_t * p) {
  /* Free object reference first (object might be destructed but not yet freed) */
  if (p->ob)
    {
      free_object (p->ob, "free_sentence");
      p->ob = 0;
    }

  if (p->flags & V_FUNCTION)
    {
      if (p->function.f)
        free_funp (p->function.f);
      p->function.f = 0;
    }
  else
    {
      if (p->function.s)
        free_string (p->function.s);
      p->function.s = 0;
    }

  if (p->verb)
    {
      free_string (p->verb);
      p->verb = 0;
    }

  /* free carryover args if present */
  if (p->args)
    {
      free_array (p->args);
      p->args = NULL;
    }

  p->flags = 0;
  p->next = sent_free;
  sent_free = p;
}

/**
 * @brief Deallocate an object structure.
 * 
 * This function is called when the reference count of an object
 * reaches zero.
 * @param ob The object to deallocate.
 * @param from A string indicating where the deallocation was initiated from.
 */
void dealloc_object (object_t * ob, const char *from) {
  sentence_t *s;

  if (!(ob->flags & O_DESTRUCTED))
    {
      /* This is fatal, and should never happen. */
      fatal ("FATAL: Object 0x%x /%s ref count 0, but not destructed (from %s).\n", ob, ob->name, from);
    }
  DEBUG_CHECK (ob->interactive, "Tried to free an interactive object.\n");

  opt_trace (TT_EVAL, "freeing object \"/%s\"", ob->name);
  /*
   * If the program is freed, then we can also free the variable
   * declarations.
   */
  if (ob->prog)
    {
      tot_alloc_object_size -= (ob->prog->num_variables_total - 1) * sizeof (svalue_t) + sizeof (object_t);
      free_prog (ob->prog, 1);
      ob->prog = 0;
    }
  /* Free sentences if not already freed during destruct_object().
   * With the fix to free sentences earlier, ob->sent should already be NULL.
   * This code remains as a safety net for backwards compatibility. */
  if (ob->sent)
    {
      for (s = ob->sent; s;)
        {
          sentence_t *next = s->next;
          free_sentence (s);
          s = next;
        }
      ob->sent = NULL;
    }
#ifdef PRIVS
  if (ob->privs)
    free_string (ob->privs);
#endif
  if (ob->name)
    {
      opt_trace (TT_MEMORY|3, "freed object name \"/%s\"", ob->name);
      DEBUG_CHECK1 (lookup_object_hash (ob->name) == ob,
                    "Freeing object /%s but name still in name table",
                    ob->name);
      FREE (ob->name);
      ob->name = 0;
    }
  tot_alloc_object--;
  FREE ((char *) ob);
}

/**
 * @brief Decrease the reference count of an object, and deallocate it if it reaches zero.
 * @param ob The object to free.
 * @param from A string indicating where the free was initiated from.
 */
void free_object (object_t * ob, const char *from) {
  opt_trace (TT_MEMORY|3, "releasing object name \"/%s\" (ref=%d)", ob->name, ob->ref - 1);
  if (--ob->ref > 0)
    return;
  dealloc_object (ob, from);
}

/*
 * Allocate an empty object, and set all variables to 0. Note that a
 * 'object_t' already has space for one variable. So, if no variables
 * are needed, we allocate a space that is smaller than 'object_t'. This
 * unused (last) part must of course (and will not) be referenced.
 */
object_t* get_empty_object (int num_var) {
  static object_t NULL_object;
  object_t *ob;
  int size = sizeof (object_t) + (num_var - !!num_var) * sizeof (svalue_t);
  int i;

  tot_alloc_object++;
  tot_alloc_object_size += size;
  ob = (object_t *) DXALLOC (size, TAG_OBJECT, "get_empty_object");
  /*
   * marion Don't initialize via memset, this is incorrect. E.g. the bull
   * machines have a (char *)0 which is not zero. We have structure
   * assignment, so use it.
   */
  *ob = NULL_object;
  ob->ref = 1;
  for (i = 0; i < num_var; i++)
    ob->variables[i] = const0u;
  return ob;
}

object_t **hashed_living;

static int num_living_names, num_searches = 1, search_length = 1;

static int hash_living_name (char *str) {
  return whashstr (str, 20) % CONFIG_INT (__LIVING_HASH_TABLE_SIZE__);
}

object_t* find_living_object (char *str, int user) {
  object_t **obp, *tmp;
  object_t **hl;

  if (!str)
    return 0;
  num_searches++;
  hl = &hashed_living[hash_living_name (str)];
  for (obp = hl; *obp; obp = &(*obp)->next_hashed_living)
    {
      search_length++;
      if ((*obp)->flags & O_HIDDEN)
        {
          if (!valid_hide (current_object))
            continue;
        }
      if (user && !((*obp)->flags & O_ONCE_INTERACTIVE))
        continue;
      if (!((*obp)->flags & O_ENABLE_COMMANDS))
        continue;
      if (strcmp ((*obp)->living_name, str) == 0)
        break;
    }
  if (*obp == 0)
    return 0;
  /* Move the found ob first. */
  if (obp == hl)
    return *obp;
  tmp = *obp;
  *obp = tmp->next_hashed_living;
  tmp->next_hashed_living = *hl;
  *hl = tmp;
  return tmp;
}

void set_living_name (object_t * ob, char *str) {
  object_t **hl;

  if (ob->flags & O_DESTRUCTED)
    return;
  if (ob->living_name)
    {
      remove_living_name (ob);
    }
  num_living_names++;
  hl = &hashed_living[hash_living_name (str)];
  ob->next_hashed_living = *hl;
  *hl = ob;
  ob->living_name = make_shared_string (str);
  return;
}

void remove_living_name (object_t * ob) {
  object_t **hl;

  num_living_names--;
  DEBUG_CHECK (!ob->living_name, "remove_living_name: no living name set.\n");
  hl = &hashed_living[hash_living_name (ob->living_name)];
  while (*hl)
    {
      if (*hl == ob)
        break;
      hl = &(*hl)->next_hashed_living;
    }
  DEBUG_CHECK1 (*hl == 0,
                "remove_living_name: Object named %s no in hash list.\n",
                ob->living_name);
  *hl = ob->next_hashed_living;
  free_string (ob->living_name);
  ob->next_hashed_living = 0;
  ob->living_name = 0;
}

void stat_living_objects (outbuffer_t * out) {
  outbuf_add (out, "Hash table of living objects:\n");
  outbuf_add (out, "-----------------------------\n");
  outbuf_addv (out, "%d living named objects, average search length: %4.2f\n",
               num_living_names, (double) search_length / num_searches);
}

void reset_object (object_t * ob) {
  object_t *save_command_giver;

  if (CONFIG_INT (__TIME_TO_RESET__) > 0)
    {
      /* Be sure to update time first ! */
      ob->next_reset = current_time + CONFIG_INT (__TIME_TO_RESET__) / 2 +
        rand () % (CONFIG_INT (__TIME_TO_RESET__) / 2);
    }

  save_command_giver = command_giver;
  command_giver = (object_t *) 0;
  if (!apply (APPLY_RESET, ob, 0, ORIGIN_DRIVER))
    {
      /* no reset() in the object */
      ob->flags &= ~O_WILL_RESET;	/* don't call it next time */
    }
  command_giver = save_command_giver;
  ob->flags |= O_RESET_STATE;
}

/* Reason for the following 1. save cache space 2. speed :) */
/* The following is to be called only from reset_object for */
/* otherwise extra checks are needed - Sym                  */

static void call___INIT (object_t * ob) {

  program_t *progp;
  compiler_function_t *cfp;
  int num_functions;

  /* No try_reset here for obvious reasons :) */
  ob->flags &= ~O_RESET_STATE;

  progp = ob->prog;
  num_functions = progp->num_functions_defined;
  if (!num_functions)
    return;

  /* ___INIT turns out to be always the last function */
  cfp = &progp->function_table[num_functions - 1];
  if (cfp->name[0] != APPLY___INIT_SPECIAL_CHAR)
    return;
  push_control_stack (FRAME_FUNCTION | FRAME_OB_CHANGE);
  current_prog = progp;
  csp->fr.table_index = num_functions - 1;
  caller_type = ORIGIN_DRIVER;
  csp->num_local_variables = 0;

  setup_new_frame (cfp->runtime_index);
  previous_ob = current_object;

  current_object = ob;
  opt_trace (TT_EVAL, "(obsoleted) calling __INIT");
  call_program (current_prog, cfp->address);
  sp--;
}

void call_create (object_t * ob, int num_arg) {
  if (CONFIG_INT (__TIME_TO_RESET__) > 0)
    {
      /* Be sure to update time first ! */
      ob->next_reset = current_time + CONFIG_INT (__TIME_TO_RESET__) / 2 +
        rand () % (CONFIG_INT (__TIME_TO_RESET__) / 2);
    }

  call___INIT (ob);

  if (ob->flags & O_DESTRUCTED)
    {
      pop_n_elems (num_arg);
      return;			/* sigh */
    }

  apply (APPLY_CREATE, ob, num_arg, ORIGIN_DRIVER);

  ob->flags |= O_RESET_STATE;
}

int object_visible (object_t * ob) {
  if (ob->flags & O_HIDDEN)
    {
      if (current_object->flags & O_HIDDEN)
        {
          return 1;
        }
      return valid_hide (current_object);
    }
  else
    {
      return 1;
    }
}

void reload_object (object_t * obj) {
  int i;

  if (!obj->prog)
    return;
  for (i = 0; i < (int) obj->prog->num_variables_total; i++)
    {
      free_svalue (&obj->variables[i], "reload_object");
      obj->variables[i] = const0u;
    }

  if (obj->flags & O_EFUN_SOCKET)
    {
      close_referencing_sockets (obj);
    }

  if (obj->living_name)
    remove_living_name (obj);
  obj->flags &= ~O_ENABLE_COMMANDS;
  set_heart_beat (obj, 0);
  remove_all_call_out (obj);

  obj->euid = NULL;
  call_create (obj, 0);
}

void init_objects () {
  hashed_living = (object_t **) DCALLOC (
    CONFIG_INT (__LIVING_HASH_TABLE_SIZE__), sizeof (object_t *),
    TAG_LIVING, "init_objects"
  );
  if (!hashed_living) 
    {
      debug_perror ("init_objects", 0);
      debug_fatal ("Cannot initialize living objects hash table.\n");
      exit (EXIT_FAILURE);
    }
}

void deinit_objects () {
  if (hashed_living)
    {
      if (num_living_names > 0)
        debug_message ("Warning: deinit_objects with %d living names still set.\n", num_living_names);
      FREE (hashed_living);
      hashed_living = NULL;
    }
  while (sent_free)
    {
      sentence_t *next = sent_free->next;
      FREE (sent_free);
      sent_free = next;
    }
  if (tot_alloc_object)
    debug_warn ("Memory leak: %zu objects still allocated at shutdown.\n", tot_alloc_object);
}
