#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "rc.h"
#include "src/interpret.h"
#include "src/comm.h"
#include "src/command.h"
#include "qsort.h"

#include "array.h"
#include "object.h"
#include "otable.h"
#include "program.h"
#include "lpc/include/origin.h"
#include "svalue.h"

#define ALLOC_ARRAY(nelem) \
    (array_t *)DXALLOC(sizeof (array_t) + \
	  sizeof(svalue_t) * (nelem - 1), TAG_ARRAY, "ALLOC_ARRAY")
#define RESIZE_ARRAY(vec, nelem) \
    (array_t *)DREALLOC(vec, sizeof (array_t) + \
	  sizeof(svalue_t) * (nelem - 1), TAG_ARRAY, "RESIZE_ARRAY")

#ifdef ARRAY_STATS
int num_arrays;
size_t total_array_size;
#endif

static int builtin_sort_array_cmp_fwd (void *, void *);
static int builtin_sort_array_cmp_rev (void *, void *);
static int sort_array_cmp (void *, void *);
static int deep_inventory_count (object_t *);
static void deep_inventory_collect (object_t *, array_t *, int *);
static int alist_cmp (svalue_t *, svalue_t *);

/*
 * Make an empty array for everyone to use, never to be deallocated.
 * It is cheaper to reuse it, than to use MALLOC() and allocate.
 */

array_t the_null_array = {
  .ref = 1,	/* Ref count, which will ensure that it will never be deallocated */
  .size = 0,	/* size */
};

/*
 * Allocate an array of size 'n'.
 */
array_t* allocate_array (size_t n) {

  array_t *p;

  if (n > (size_t)CONFIG_INT (__MAX_ARRAY_SIZE__))
    error ("Illegal array size.\n");
  if (n == 0)
    {
      return &the_null_array;
    }
#ifdef ARRAY_STATS
  num_arrays++;
  total_array_size += sizeof (array_t) + sizeof (svalue_t) * (n - 1);
#endif
  p = ALLOC_ARRAY (n);
  p->ref = 1;
  p->size = (unsigned short)n;
  while (n--)
    p->item[n] = const0;
  return p;
}

array_t* allocate_empty_array (size_t n) {

  array_t *p;

  if (n > (size_t)CONFIG_INT (__MAX_ARRAY_SIZE__))
    error ("Illegal array size.\n");
  if (!n)
    return &the_null_array;
#ifdef ARRAY_STATS
  num_arrays++;
  total_array_size += sizeof (array_t) + sizeof (svalue_t) * (n - 1);
#endif
  p = ALLOC_ARRAY (n);
  p->ref = 1;
  p->size = (unsigned short)n;
  while (n--)
    p->item[n] = const0;
  return p;
}

void
dealloc_array (array_t * p)
{
  int i;

  if (p == &the_null_array)
    return;

  for (i = p->size; i--;)
    free_svalue (&p->item[i], "free_array");
#ifdef ARRAY_STATS
  num_arrays--;
  total_array_size -= sizeof (array_t) + sizeof (svalue_t) * (p->size - 1);
#endif
  FREE ((char *) p);
}

void
free_array (array_t * p)
{
  if (--(p->ref) > 0)
    return;

  dealloc_array (p);
}

void
free_empty_array (array_t * p)
{
  if ((--(p->ref) > 0) || (p == &the_null_array))
    {
      return;
    }
#ifdef ARRAY_STATS
  num_arrays--;
  total_array_size -= sizeof (array_t) + sizeof (svalue_t) * (p->size - 1);
#endif
  FREE ((char *) p);
}

/*
 * When a array is given as argument to an efun, all items have to be
 * checked if there would be a destructed object.
 * A bad problem currently is that a array can contain another array, so this
 * should be tested too. But, there is currently no prevention against
 * recursive arrays, which means that this can not be tested. Thus, MudOS
 * may crash if a array contains a array that contains a destructed object
 * and this top-most array is used as an argument to an efun.
 */
/* MudOS won't crash when doing simple operations like assign_svalue
 * on a destructed object. You have to watch out, of course, that you don't
 * apply a function to it.
 * to save space it is preferable that destructed objects are freed soon.
 *   amylaar
 */
void check_for_destr (array_t * v) {
  int i = v->size;

  while (i--)
    {
      if ((v->item[i].type == T_OBJECT) && (v->item[i].u.ob->flags & O_DESTRUCTED))
        {
          free_svalue (&v->item[i], "check_for_destr");
          v->item[i] = const0;
        }
    }
}

/**
 * Split a string into sub-strings separated by delimiter, return an array of sub-strings.
 */
array_t* explode_string (const char *str, size_t slen, const char *del, size_t len) {
  const char *p, *beg, *end;
#ifndef REVERSIBLE_EXPLODE_STRING
  const char *lastdel = (const char *) NULL;
#endif
  int num, j, limit;
  array_t *ret;
  char *buff, *tmp;

  if (!slen)
    return &the_null_array;

  /* return an array of length strlen(str) -w- one character per element */
  if (len == 0)
    {
      size_t slen_wcs = mbstowcs (NULL, str, 0);
      int mb;

      if (slen_wcs == (size_t) -1)
        {
          error ("An invalid multibyte sequence is encountered.");
          return &the_null_array;
        }
      if (slen_wcs > (size_t)CONFIG_INT (__MAX_ARRAY_SIZE__))
        {
          slen_wcs = (size_t)CONFIG_INT (__MAX_ARRAY_SIZE__);
        }
      ret = allocate_empty_array (slen_wcs);
      for (j = 0; j < (int)slen_wcs; j++)
        {
          mb = mblen (str, slen);	/* get length of a multibyte character */
          if (-1 == mb)
            {
              debug_warn ("invalid multibyte character in string");
              break;
            }
          else if (0 == mb)
            break;
          SET_SVALUE_MALLOC_STRING (&ret->item[j], tmp = new_string (mb, "explode_string: tmp"));
          memcpy (tmp, str, mb);
          tmp[mb] = '\0';
          str += mb;
          slen -= mb;
        }
      return ret;
    }

  /* length of delimiter > 0 */
  if (mblen(del, len) == -1)
    {
      debug_warn ("An invalid multibyte sequence is encountered in delimiter.");
      // return &the_null_array;
    }

#ifndef REVERSIBLE_EXPLODE_STRING
  /*
   * Skip leading 'del' strings, if any.
   */
  while (strncmp (str, del, len) == 0)
    {
      str += len;
      slen -= len;
      if (str[0] == '\0')
        {
          return &the_null_array;
        }
#  ifdef SANE_EXPLODE_STRING
      break;
#  endif
    }
#endif

  /*
   * Find number of occurences of the delimiter 'del'.
   */
  for (p = str, end = str + slen, num = 0; *p;)
    {
      /* Advance one multibyte character, don't compare with
          delimiter in the middle of a multibyte character */
      int mb = mblen (p, end - p);
      if (((int)len >= mb) && (strncmp (p, del, len) == 0))
        {
          num++;
#ifndef REVERSIBLE_EXPLODE_STRING
          lastdel = p;
#endif
          p += len;
        }
      else
        {
          if (mb > 0)
            p += mb;
          else
            break;
        }
    }

  /*
   * Compute number of array items. It is either number of delimiters, or,
   * one more.
   */
#ifdef REVERSIBLE_EXPLODE_STRING
  num++;
#else
  if ((slen <= len) || (lastdel != (str + slen - len)))
    {
      num++;
    }
#endif
  if (num > CONFIG_INT (__MAX_ARRAY_SIZE__))
    {
      num = CONFIG_INT (__MAX_ARRAY_SIZE__);
    }
  ret = allocate_empty_array (num);
  limit = CONFIG_INT (__MAX_ARRAY_SIZE__) - 1;	/* extra element can be added after loop */
  for (p = str, beg = str, end = str + slen, num = 0; *p && (num < limit);)
    {
      /* Advance one multibyte character, don't compare with
          delimiter in the middle of a multibyte character */
      int mb = mblen (p, end - p);
      if (((int)len >= mb) && (strncmp (p, del, len) == 0))
        {
          if (num >= ret->size)
            fatal ("Index out of bounds in explode!\n");

          SET_SVALUE_MALLOC_STRING (&ret->item[num], buff = new_string (p - beg, "explode_string: buff"));

          strncpy (buff, beg, p - beg);
          buff[p - beg] = '\0';
          num++;
          beg = p + len;
          p = beg;
        }
      else
        {
          if (mb > 0)
            p += mb;
          else
            break;
        }
    }

  /* Copy last occurence, if there was not a 'del' at the end. */
#ifdef REVERSIBLE_EXPLODE_STRING
  SET_SVALUE_MALLOC_STRING (&ret->item[num],
                            string_copy (beg, "explode_string: last, len != 1"));
#else
  if (*beg != '\0')
    {
      SET_SVALUE_MALLOC_STRING (&ret->item[num],
                                string_copy (beg, "explode_string: last, len != 1"));
    }
#endif
  return ret;
}

malloc_str_t implode_string (array_t * arr, const char *del, size_t del_len) {

  size_t size;
  int i, num;
  char *p, *q;
  svalue_t *sv = arr->item;

  for (i = arr->size, size = 0, num = 0; i--;)
    {
      if (sv[i].type == T_STRING)
        {
          size += SVALUE_STRLEN (&sv[i]);
          num++;
        }
    }
  if (num == 0)
    return string_copy ("", "implode_string");

  p = new_string (size + (num - 1) * del_len, "implode_string: p");
  q = p;
  for (i = 0, num = 0; i < arr->size; i++)
    {
      if (sv[i].type == T_STRING)
        {
          if (num)
            {
              strncpy (p, del, del_len);
              p += del_len;
            }
          size = SVALUE_STRLEN (&sv[i]);
          strncpy (p, SVALUE_STRPTR(&sv[i]), size);
          p += size;
          num++;
        }
    }
  *p = 0;
  return q;
}

/**
  * Implode an array using a function pointer.
  * The function pointer is called with two arguments, both strings,
  * and must return a string.
  * The first argument is the accumulated string so far, the second
  * argument is the next array element.
  * If the first_on_stack flag is set, the first argument is the
  * next array element, and the accumulated string is on the stack.
  * This is to allow for efuns like reduce() to be implemented.
  */
void implode_array (funptr_t * funp, array_t * arr, svalue_t * dest, int first_on_stack) {

  int i = 0, n;
  svalue_t *v;

  if (first_on_stack)
    {
      if (!(n = arr->size))
        {
          *dest = *sp--;
          return;
        }
    }
  else
    {
      if (!(n = arr->size))
        {
          *dest = const0;
          return;
        }
      else if (n == 1)
        {
          assign_svalue_no_free (dest, &arr->item[0]);
          return;
        }
    }

  if (!first_on_stack)
    push_svalue (&arr->item[i++]);

  while (1)
    {
      push_svalue (&arr->item[i++]);
      v = CALL_FUNCTION_POINTER_SLOT_CALL (funp, 2);

      if (!v)
        {
          CALL_FUNCTION_POINTER_SLOT_FINISH();
          *dest = const0;
          return;
        }
      if (i < n)
        {
          push_svalue (v);
          CALL_FUNCTION_POINTER_SLOT_FINISH();
        }
      else
        break;
    }

  assign_svalue_no_free (dest, v);
  CALL_FUNCTION_POINTER_SLOT_FINISH();
}

/*
 * Slice of an array.
 * It now frees the passed array
 */
array_t* slice_array (array_t * p, int from, int to) {
  int cnt;
  svalue_t *sv1, *sv2;

  if (from < 0)
    from = 0;
  if (to >= p->size)
    to = p->size - 1;
  if (from > to)
    {
      free_array (p);
      return &the_null_array;
    }

  if (!(--p->ref))
    {
#ifdef ARRAY_STATS
      total_array_size += (to - from + 1 - p->size) * sizeof (svalue_t);
#endif
      if (from)
        {
          sv1 = p->item + from;
          cnt = from;
          while (cnt--)
            free_svalue (--sv1, "slice_array:2");
          cnt = to - from + 1;
          sv1 = p->item;
          sv2 = p->item + from;
          while (cnt--)
            *sv1++ = *sv2++;
        }
      else
        {
          sv2 = p->item + to + 1;
        }
      cnt = (p->size - 1) - to;
      while (cnt--)
        free_svalue (sv2++, "slice_array:3");
      p = RESIZE_ARRAY (p, to - from + 1);
      p->size = (unsigned short)(to - from + 1);
      p->ref = 1;
      return p;
    }
  else
    {
      array_t *d;

      d = allocate_empty_array (to - from + 1);
      sv1 = d->item - from;
      sv2 = p->item;
      for (cnt = from; cnt <= to; cnt++)
        assign_svalue_no_free (sv1 + cnt, sv2 + cnt);
      return d;
    }
}

/*
 * Copy of an array
 */
array_t *
copy_array (array_t * p)
{
  array_t *d;
  int n;
  svalue_t *sv1 = p->item, *sv2;

  d = allocate_empty_array (n = p->size);
  sv2 = d->item;
  while (n--)
    assign_svalue_no_free (sv2 + n, sv1 + n);
  return d;
}

/* EFUN: filter_array

   Runs all elements of an array through ob->func()
   and returns an array holding those elements that ob->func
   returned 1 for.
   */

#ifdef F_FILTER
void
filter_array (svalue_t * arg, int num_arg)
{
  array_t *vec = arg->u.arr, *r;
  int size;

  if ((size = vec->size) < 1)
    {
      pop_n_elems (num_arg - 1);
      return;
    }
  else
    {
      char *flags;
      svalue_t *v;
      int res = 0, cnt;
      function_to_call_t ftc;

      process_efun_callback (1, &ftc, F_FILTER);

      flags = new_string (size, "TEMP: filter: flags");
      push_malloced_string (flags);

      for (cnt = 0; cnt < size; cnt++)
        {
          push_svalue (vec->item + cnt);
          v = call_efun_callback (&ftc, 1);
          if (!IS_ZERO (v))
            {
              flags[cnt] = 1;
              res++;
            }
          else
            flags[cnt] = 0;
          call_efun_callback_finish (&ftc);
        }
      r = allocate_empty_array (res);
      if (res)
        {
          while (cnt--)
            {
              if (flags[cnt])
                assign_svalue_no_free (&r->item[--res], vec->item + cnt);
            }
        }

      FREE_MSTR (flags);
      sp--;
      pop_n_elems (num_arg - 1);
      free_array (vec);
      sp->u.arr = r;
    }
}
#endif

/* Unique maker

   These routines takes an array of objects and calls the function 'func'
   in them. The return values are used to decide which of the objects are
   unique. Then an array on the below form are returned:

   ({
   ({Same1:1, Same1:2, Same1:3, .... Same1:N }),
   ({Same2:1, Same2:2, Same2:3, .... Same2:N }),
   ({Same3:1, Same3:2, Same3:3, .... Same3:N }),
   ....
   ....
   ({SameM:1, SameM:2, SameM:3, .... SameM:N }),
   })

   i.e an array of arrays consisting of lists of objectpointers
   to all the nonunique objects for each unique set of objects.

   The basic purpose of this routine is to speed up the preparing of the
   array used for describing.

   */

/* nonstatic, is used in mappings too */
int
sameval (svalue_t * arg1, svalue_t * arg2)
{
  DEBUG_CHECK (!arg1 || !arg2, "Null pointer passed to sameval.\n");

  switch (arg1->type | arg2->type)
    {
    case T_NUMBER:
      return arg1->u.number == arg2->u.number;
    case T_ARRAY:
    case T_CLASS:
      return arg1->u.arr == arg2->u.arr;
    case T_STRING:
      if (string_length_differs (arg1, arg2))
        return 0;
      return svalue_string_lexcmp (arg1, arg2) == 0;
    case T_OBJECT:
      return arg1->u.ob == arg2->u.ob;
    case T_MAPPING:
      return arg1->u.map == arg2->u.map;
    case T_FUNCTION:
      return arg1->u.fp == arg2->u.fp;
    case T_REAL:
      return arg1->u.real == arg2->u.real;
    case T_BUFFER:
      return arg1->u.buf == arg2->u.buf;
    }
  return 0;
}

#ifdef F_UNIQUE_ARRAY

typedef struct unique_s
{
  svalue_t mark;
  int count;
  struct unique_s *next;
  int *indices;
}
unique_t;

typedef struct unique_list_s
{
  unique_t *head;
  struct unique_list_s *next;
}
unique_list_t;

static unique_list_t *g_u_list = 0;

void
unique_array_error_handler (void)
{
  unique_list_t *unlist = g_u_list;
  unique_t *uptr = unlist->head, *nptr;

  g_u_list = g_u_list->next;
  while (uptr)
    {
      nptr = uptr->next;
      FREE ((char *) uptr->indices);
      free_svalue (&uptr->mark, "unique_array_error_handler");
      FREE ((char *) uptr);
      uptr = nptr;
    }
  FREE ((char *) unlist);
}

void
f_unique_array (void)
{
  array_t *v, *ret;
  int size, i, numkeys = 0, *ind, num_arg = st_num_arg;
  svalue_t *skipval, *sv, *svp;
  unique_list_t *unlist;
  unique_t **head, *uptr, *nptr;
  funptr_t *funp = 0;
  const char *func = NULL;

  size = (v = (sp - num_arg + 1)->u.arr)->size;
  if (!size)
    {
      if (num_arg == 3)
        free_svalue (sp--, "f_unique_array");
      free_svalue (sp--, "f_unique_array");
      return;
    }

  if (num_arg == 3)
    {
      skipval = sp;
      if ((sp - 1)->type == T_FUNCTION)
        funp = (sp - 1)->u.fp;
      else
        func = SVALUE_STRPTR(sp - 1);
    }
  else
    {
      skipval = &const0;
      if (sp->type == T_FUNCTION)
        funp = sp->u.fp;
      else
        func = SVALUE_STRPTR(sp);
    }

  unlist = ALLOCATE (unique_list_t, TAG_TEMPORARY, "f_unique_array:1");
  unlist->next = g_u_list;
  unlist->head = 0;
  head = &unlist->head;
  g_u_list = unlist;

  (++sp)->type = T_ERROR_HANDLER;
  sp->u.error_handler = unique_array_error_handler;

  for (i = 0; i < size; i++)
    {
      if (funp)
        {
          push_svalue (v->item + i);
          sv = CALL_FUNCTION_POINTER_SLOT_CALL (funp, 1);
          if (sv && !sameval (sv, skipval))
            {
              uptr = *head;
              while (uptr)
                {
                  if (sameval (sv, &uptr->mark))
                    {
                      uptr->indices = RESIZE (uptr->indices, uptr->count + 1, int,
                                              TAG_TEMPORARY, "f_unique_array:2");
                      uptr->indices[uptr->count++] = i;
                      break;
                    }
                  uptr = uptr->next;
                }
              if (!uptr)
                {
                  numkeys++;
                  uptr = ALLOCATE (unique_t, TAG_TEMPORARY, "f_unique_array:3");
                  uptr->indices = ALLOCATE (int, TAG_TEMPORARY, "f_unique_array:4");
                  uptr->count = 1;
                  uptr->indices[0] = i;
                  uptr->next = *head;
                  assign_svalue_no_free (&uptr->mark, sv);
                  *head = uptr;
                }
            }
          CALL_FUNCTION_POINTER_SLOT_FINISH();
          continue;
        }
      else if ((v->item + i)->type == T_OBJECT)
        {
          sv = APPLY_SLOT_CALL (func, (v->item + i)->u.ob, 0, ORIGIN_EFUN);
          if (sv && !sameval (sv, skipval))
            {
              uptr = *head;
              while (uptr)
                {
                  if (sameval (sv, &uptr->mark))
                    {
                      uptr->indices = RESIZE (uptr->indices, uptr->count + 1, int,
                                              TAG_TEMPORARY, "f_unique_array:2");
                      uptr->indices[uptr->count++] = i;
                      break;
                    }
                  uptr = uptr->next;
                }
              if (!uptr)
                {
                  numkeys++;
                  uptr = ALLOCATE (unique_t, TAG_TEMPORARY, "f_unique_array:3");
                  uptr->indices = ALLOCATE (int, TAG_TEMPORARY, "f_unique_array:4");
                  uptr->count = 1;
                  uptr->indices[0] = i;
                  uptr->next = *head;
                  assign_svalue_no_free (&uptr->mark, sv);
                  *head = uptr;
                }
            }
          APPLY_SLOT_FINISH_CALL();
          continue;
        }
      else
        sv = 0;

      if (sv && !sameval (sv, skipval))
        {
          uptr = *head;
          while (uptr)
            {
              if (sameval (sv, &uptr->mark))
                {
                  uptr->indices = RESIZE (uptr->indices, uptr->count + 1, int,
                                          TAG_TEMPORARY, "f_unique_array:2");
                  uptr->indices[uptr->count++] = i;
                  break;
                }
              uptr = uptr->next;
            }
          if (!uptr)
            {
              numkeys++;
              uptr = ALLOCATE (unique_t, TAG_TEMPORARY, "f_unique_array:3");
              uptr->indices = ALLOCATE (int, TAG_TEMPORARY, "f_unique_array:4");
              uptr->count = 1;
              uptr->indices[0] = i;
              uptr->next = *head;
              assign_svalue_no_free (&uptr->mark, sv);
              *head = uptr;
            }
        }
    }

  ret = allocate_empty_array (numkeys);
  uptr = *head;
  svp = v->item;
  while (numkeys--)
    {
      nptr = uptr->next;
      (sv = ret->item + numkeys)->type = T_ARRAY;
      sv->u.arr = allocate_empty_array (i = uptr->count);
      skipval = sv->u.arr->item + i;
      ind = uptr->indices;
      while (i--)
        {
          assign_svalue_no_free (--skipval, svp + ind[i]);
        }
      FREE ((char *) ind);
      free_svalue (&uptr->mark, "f_unique_array");
      FREE ((char *) uptr);
      uptr = nptr;
    }

  unlist = g_u_list->next;
  FREE ((char *) g_u_list);
  g_u_list = unlist;
  sp--;
  pop_n_elems (num_arg - 1);
  free_array (v);
  sp->u.arr = ret;
}

/*
 * End of Unique maker
 *************************
 */
#endif

/* Concatenation of two arrays into one
 */
array_t *
add_array (array_t * p, array_t * r)
{
  int cnt, res;
  array_t *d;			/* destination */

  /*
   * have to be careful with size zero arrays because they could be
   * the_null_array.  REALLOC(the_null_array, ...) is bad :(
   */
  if (p->size == 0)
    {
      p->ref--;
      return r->ref > 1 ? (r->ref--, copy_array (r)) : r;
    }
  if (r->size == 0)
    {
      r->ref--;
      return p->ref > 1 ? (p->ref--, copy_array (p)) : p;
    }

  res = p->size + r->size;
  if (res < 0 || res > CONFIG_INT (__MAX_ARRAY_SIZE__))
    error ("result of array addition is greater than maximum array size.\n");

  /* x += x */
  if ((p == r) && (p->ref == 2))
    {
      d = RESIZE_ARRAY (p, res);
      if (!d)
        fatal ("Out of memory.\n");
      /* copy myself */
      for (cnt = d->size; cnt--;)
        assign_svalue_no_free (&d->item[--res], &d->item[cnt]);
#ifdef ARRAY_STATS
      total_array_size += sizeof (svalue_t) * (d->size);
#endif
      d->ref = 1;
      d->size <<= 1;

      return d;
    }

  /* transfer svalues for ref 1 target array */
  if (p->ref == 1)
    {
      /*
       * realloc(p) to try extending block; this will save an
       * allocate_array(), copying the svalues over, and free()'ing p
       */
      d = RESIZE_ARRAY (p, res);
      if (!d)
        fatal ("Out of memory.\n");

#ifdef ARRAY_STATS
      total_array_size += sizeof (svalue_t) * (r->size);
#endif
      /* d->ref = 1;     d is p, and p's ref was already one -Beek */
      d->size = (unsigned short)res;
    }
  else
    {
      d = allocate_empty_array (res);

      for (cnt = p->size; cnt--;)
        assign_svalue_no_free (&d->item[cnt], &p->item[cnt]);
      p->ref--;
    }

  /* transfer svalues from ref 1 source array */
  if (r->ref == 1)
    {
      for (cnt = r->size; cnt--;)
        d->item[--res] = r->item[cnt];
#ifdef ARRAY_STATS
      num_arrays--;
      total_array_size -= sizeof (array_t) +
        sizeof (svalue_t) * (r->size - 1);
#endif
      FREE ((char *) r);
    }
  else
    {
      for (cnt = r->size; cnt--;)
        assign_svalue_no_free (&d->item[--res], &r->item[cnt]);
      r->ref--;
    }

  return d;
}

/* Runs all elements of an array through ob::func
   and replaces each value in arr by the value returned by ob::func
   */
#ifdef F_MAP
void
map_array (svalue_t * arg, int num_arg)
{
  array_t *arr = arg->u.arr;
  array_t *r;
  int size;

  if ((size = arr->size) < 1)
    r = &the_null_array;
  else
    {
      function_to_call_t ftc;
      int cnt;
      svalue_t *v;

      process_efun_callback (1, &ftc, F_MAP);

      r = allocate_array (size);

      (++sp)->type = T_ARRAY;
      sp->u.arr = r;

      for (cnt = 0; cnt < size; cnt++)
        {
          push_svalue (arr->item + cnt);
          v = call_efun_callback (&ftc, 1);
          if (v)
            assign_svalue_no_free (&r->item[cnt], v);
          else
            {
              call_efun_callback_finish (&ftc);
              break;
            }
          call_efun_callback_finish (&ftc);
        }
      sp--;
    }

  pop_n_elems (num_arg);
  (++sp)->type = T_ARRAY;
  sp->u.arr = r;
}

void
map_string (svalue_t * arg, int num_arg)
{
  char *arr;
  char *p;
  funptr_t *funp = 0;
  int numex = 0;
  object_t *ob = NULL;
  svalue_t *extra = NULL, *v;
  const char *func = NULL;

  /* get a modifiable string */
  /* do not use arg after this; it has been copied or freed.
     Put our result string on the stack where it belongs in case of an
     error (note it is also in the right spot for the return value).
   */
  unlink_string_svalue (arg);
  arr = arg->u.malloc_string;

  if (arg[1].type == T_FUNCTION)
    {
      funp = arg[1].u.fp;
      if (num_arg > 2)
        extra = arg + 2, numex = num_arg - 2;
    }
  else
    {
      func = SVALUE_STRPTR(&arg[1]);
      if (num_arg < 3)
        ob = current_object;
      else
        {
          if (arg[2].type == T_OBJECT)
            ob = arg[2].u.ob;
          else if (arg[2].type == T_STRING)
            {
              if ((ob = find_or_load_object (SVALUE_STRPTR(&arg[2])))
                  && !object_visible (ob))
                ob = 0;
            }
          if (num_arg > 3)
            extra = arg + 3, numex = num_arg - 3;
          if (!ob)
            error ("Bad argument 3 to map_string.\n");
        }
    }

  for (p = arr; *p; p++)
    {
      int used_funp;

      push_number ((unsigned char) *p);
      if (numex)
        push_some_svalues (extra, numex);
      if (funp)
        {
          used_funp = 1;
          v = CALL_FUNCTION_POINTER_SLOT_CALL (funp, numex + 1);
        }
      else
        {
          used_funp = 0;
          v = APPLY_SLOT_CALL (func, ob, 1 + numex, ORIGIN_EFUN);
        }
      /* no function or illegal return value is unaltered.
       * Anyone got a better idea?  A few idea:
       * (1) insert strings? - algorithm needs changing
       * (2) ignore them? - again, could require a realloc, since size would
       *                    change
       * (3) become ' ' or something
       */
      if (!v)
        {
          if (used_funp)
            CALL_FUNCTION_POINTER_SLOT_FINISH();
          else
            APPLY_SLOT_FINISH_CALL();
          break;
        }
      if (v->type == T_NUMBER && v->u.number != 0)
        *p = ((unsigned char) (v->u.number));
      if (used_funp)
        CALL_FUNCTION_POINTER_SLOT_FINISH();
      else
        APPLY_SLOT_FINISH_CALL();
    }

  pop_n_elems (num_arg - 1);
  /* return value on stack */
}
#endif

#ifdef F_SORT_ARRAY
static function_to_call_t *sort_array_ftc;

#define COMPARE_NUMS(x,y) (x < y ? -1 : (x > y ? 1 : 0))

static array_t* builtin_sort_array (array_t * inlist, int dir) {

  quickSort ((char *) inlist->item, inlist->size, sizeof (inlist->item),
             (dir < 0) ? builtin_sort_array_cmp_rev : builtin_sort_array_cmp_fwd);

  return inlist;
}

static int builtin_sort_array_cmp_fwd (void *left, void *right) {
  svalue_t *p1 = (svalue_t *) left;
  svalue_t *p2 = (svalue_t *) right;

  switch (p1->type | p2->type)
    {
    case T_STRING:
      {
        return svalue_string_lexcmp (p1, p2);
      }

    case T_NUMBER:
      {
        return COMPARE_NUMS (p1->u.number, p2->u.number);
      }

    case T_REAL:
      {
        return COMPARE_NUMS (p1->u.real, p2->u.real);
      }

    case T_ARRAY:
      {
        array_t *v1 = p1->u.arr, *v2 = p2->u.arr;
        if (!v1->size || !v2->size)
          error ("Illegal to have empty array in array for sort_array()\n");


        switch (v1->item->type | v2->item->type)
          {
          case T_STRING:
            {
              return svalue_string_lexcmp (v1->item, v2->item);
            }

          case T_NUMBER:
            {
              return COMPARE_NUMS (v1->item->u.number, v2->item->u.number);
            }

          case T_REAL:
            {
              return COMPARE_NUMS (v1->item->u.real, v2->item->u.real);
            }
          default:
            {
              /* Temp. long err msg till I can think of a better one - Sym */
              error
                ("sort_array() cannot handle arrays of arrays whose 1st elems\naren't strings/ints/floats\n");
            }
          }
      }

    }
  error
    ("built-in sort_array() can only handle homogeneous arrays of strings/ints/floats/arrays\n");
  return 0;
}

static int builtin_sort_array_cmp_rev (void *left, void *right) {
  svalue_t *p1 = (svalue_t *) left;
  svalue_t *p2 = (svalue_t *) right;

  switch (p1->type | p2->type)
    {
    case T_STRING:
      {
        return svalue_string_lexcmp (p2, p1);
      }

    case T_NUMBER:
      {
        return COMPARE_NUMS (p2->u.number, p1->u.number);
      }

    case T_REAL:
      {
        return COMPARE_NUMS (p2->u.real, p1->u.real);
      }

    case T_ARRAY:
      {
        array_t *v1 = p1->u.arr, *v2 = p2->u.arr;
        if (!v1->size || !v2->size)
          error ("Illegal to have empty array in array for sort_array()\n");


        switch (v1->item->type | v2->item->type)
          {
          case T_STRING:
            {
              return svalue_string_lexcmp (v2->item, v1->item);
            }

          case T_NUMBER:
            {
              return COMPARE_NUMS (v2->item->u.number, v1->item->u.number);
            }

          case T_REAL:
            {
              return COMPARE_NUMS (v2->item->u.real, v1->item->u.real);
            }
          default:
            {
              /* Temp. long err msg till I can think of a better one - Sym */
              error
                ("sort_array() cannot handle arrays of arrays whose 1st elems\naren't strings/ints/floats\n");
            }
          }
      }

    }
  error
    ("built-in sort_array() can only handle homogeneous arrays of strings/ints/floats/arrays\n");
  return 0;
}

static int sort_array_cmp (void *left, void *right) {
  svalue_t *p1 = (svalue_t *) left;
  svalue_t *p2 = (svalue_t *) right;

  svalue_t *d;
  int result = 0;

  push_svalue (p1);
  push_svalue (p2);

  d = call_efun_callback (sort_array_ftc, 2);

  if (d && d->type == T_NUMBER)
    result = (int)d->u.number;
  call_efun_callback_finish (sort_array_ftc);
  return result;
}

void
f_sort_array (void)
{
  svalue_t *arg = sp - st_num_arg + 1;
  array_t *tmp = arg->u.arr;
  int num_arg = st_num_arg;

  check_for_destr (tmp);

  switch (arg[1].type)
    {
    case T_NUMBER:
      {
        tmp = builtin_sort_array (copy_array (tmp), (int)arg[1].u.number);
        break;
      }

    case T_FUNCTION:
    case T_STRING:
      {
        /* 
         * We use a global to communicate with the comparison function,
         * so we have to be careful to make sure we can recurse (the
         * callback might call sort_array itself).  For this reason, the
         * ftc structure is on the stack, and we just keep a pointer
         * to it in a global, being careful to save and restore the old
         * value.
         */
        function_to_call_t ftc, *old_ptr;

        old_ptr = sort_array_ftc;
        sort_array_ftc = &ftc;
        process_efun_callback (1, &ftc, F_SORT_ARRAY);

        tmp = copy_array (tmp);
        quickSort ((char *) tmp->item, tmp->size, sizeof (tmp->item),
                   sort_array_cmp);
        sort_array_ftc = old_ptr;
        break;
      }
    }

  pop_n_elems (num_arg);
  (++sp)->type = T_ARRAY;
  sp->u.arr = tmp;
}
#endif /* F_SORT_ARRAY */

/*
 * deep_inventory()
 *
 * This function returns the recursive inventory of an object. The returned
 * array of objects is flat, ie there is no structure reflecting the
 * internal containment relations.
 *
 * This is Robocoder's deep_inventory().  It uses two passes in order to
 * avoid costly temporary arrays (allocating, copying, adding, freeing, etc).
 * The recursive call routines are:
 *    deep_inventory_count() and deep_inventory_collect()
 */
static int valid_hide_flag;

static int
deep_inventory_count (object_t * ob)
{
  object_t *cur;
  int cnt;

  cnt = 0;

  /* step through object's inventory and count visible objects */
  for (cur = ob->contains; cur; cur = cur->next_inv)
    {
      if (cur->flags & O_HIDDEN)
        {
          if (!valid_hide_flag)
            valid_hide_flag = 1 + (valid_hide (current_object) ? 1 : 0);
          if (valid_hide_flag & 2)
            {
              cnt++;
              cnt += deep_inventory_count (cur);
            }
        }
      else
        {
          cnt++;
          cnt += deep_inventory_count (cur);
        }
    }

  return cnt;
}

static void
deep_inventory_collect (object_t * ob, array_t * inv, int *i)
{
  object_t *cur;

  /* step through object's inventory and look for visible objects */
  for (cur = ob->contains; cur; cur = cur->next_inv)
    {
      if (cur->flags & O_HIDDEN)
        {
          if (valid_hide_flag & 2)
            {
              inv->item[*i].type = T_OBJECT;
              inv->item[*i].u.ob = cur;
              (*i)++;
              add_ref (cur, "deep_inventory_collect");

              deep_inventory_collect (cur, inv, i);
            }
        }
      else
        {
          inv->item[*i].type = T_OBJECT;
          inv->item[*i].u.ob = cur;
          (*i)++;
          add_ref (cur, "deep_inventory_collect");

          deep_inventory_collect (cur, inv, i);
        }
    }
}

array_t *
deep_inventory (object_t * ob, bool take_top)
{
  array_t *dinv;
  int i;

  valid_hide_flag = 0;

  /*
   * count visible objects in an object's inventory, and in their
   * inventory, etc
   */
  i = deep_inventory_count (ob);
  if (take_top)
    i++;

  if (i == 0)
    return &the_null_array;

  /*
   * allocate an array
   */
  dinv = allocate_empty_array (i);
  if (take_top)
    {
      dinv->item[0].type = T_OBJECT;
      dinv->item[0].u.ob = ob;
      add_ref (ob, "deep_inventory");
    }
  /*
   * collect visible inventory objects recursively
   */
  i = take_top;
  deep_inventory_collect (ob, dinv, &i);

  return dinv;
}

static int alist_cmp (svalue_t * p1, svalue_t * p2) {

  if (p1->u.number != p2->u.number)
    return (int)(p1->u.number - p2->u.number);
  if (p1->type != p2->type)
    return (int)(p1->type - p2->type);
  return 0;
}

static svalue_t* alist_sort (array_t * inlist) {

  int size, j, curix, parix, child1, child2, flag;
  svalue_t *sv_tab, *tmp, *table, *sv_ptr, val;
  char *str;

  if (!(size = inlist->size))
    return (svalue_t *) NULL;
  if ((flag = (inlist->ref > 1)))
    {
      sv_tab = CALLOCATE (size, svalue_t, TAG_TEMPORARY, "alist_sort: sv_tab");
      sv_ptr = inlist->item;
      for (j = 0; j < size; j++)
        {
          if (((tmp = (sv_ptr + j))->type == T_OBJECT)
              && (tmp->u.ob->flags & O_DESTRUCTED))
            {
              free_object (tmp->u.ob, "alist_sort");
              sv_tab[j] = *tmp = const0;
            }
          else if ((tmp->type == T_STRING) && !(tmp->subtype == STRING_SHARED))
            {
              SET_SVALUE_SHARED_STRING ((tmp = sv_tab + j),
                                        make_shared_string (SVALUE_STRPTR(tmp), NULL));
            }
          else
            assign_svalue_no_free (sv_tab + j, tmp);

          if ((curix = j))
            {
              val = *tmp;

              do
                {
                  parix = (curix - 1) >> 1;
                  if (alist_cmp (sv_tab + parix, sv_tab + curix) > 0)
                    {
                      sv_tab[curix] = sv_tab[parix];
                      sv_tab[parix] = val;
                    }
                }
              while ((curix = parix));
            }
        }
    }
  else
    {
      sv_tab = inlist->item;
      for (j = 0; j < size; j++)
        {
          if (((tmp = (sv_tab + j))->type == T_OBJECT) && (tmp->u.ob->flags & O_DESTRUCTED))
            {
              free_object (tmp->u.ob, "alist_sort");
              *tmp = const0;
            }
          else if ((tmp->type == T_STRING) && !(tmp->subtype == STRING_SHARED))
            {
              str = make_shared_string(SVALUE_STRPTR(tmp), NULL);
              free_string_svalue (tmp);
              SET_SVALUE_SHARED_STRING (tmp, str);
            }

          if ((curix = j))
            {
              val = *tmp;
              do
                {
                  parix = (curix - 1) >> 1;
                  if (alist_cmp (sv_tab + parix, sv_tab + curix) > 0)
                    {
                      sv_tab[curix] = sv_tab[parix];
                      sv_tab[parix] = val;
                    }
                }
              while ((curix = parix));
            }
        }
    }

  table = CALLOCATE (size, svalue_t, TAG_TEMPORARY, "alist_sort: table");

  for (j = 0; j < size; j++)
    {
      table[j] = sv_tab[0];
      for (curix = 0;;)
        {
          child1 = (curix << 1) + 1;
          child2 = child1 + 1;

          if (child2 < size && sv_tab[child2].type != T_INVALID &&
              (sv_tab[child1].type == T_INVALID ||
               alist_cmp (sv_tab + child1, sv_tab + child2) > 0))
            {
              child1 = child2;
            }
          if (child1 < size && sv_tab[child1].type != T_INVALID)
            {
              sv_tab[curix] = sv_tab[child1];
              curix = child1;
            }
          else
            break;
        }

      sv_tab[curix].type = T_INVALID;
    }

  if (flag)
    FREE ((char *) sv_tab);
  return table;
}

array_t* subtract_array (array_t * minuend, array_t * subtrahend) {

  array_t *difference;
  svalue_t *source, *dest, *svt;
  int i, size, o, d, l, h;
  ptrdiff_t msize;

  if (!(size = subtrahend->size))
    {
      subtrahend->ref--;
      return minuend->ref > 1 ? (minuend->ref--, copy_array (minuend)) : minuend;
    }
  if (!(msize = minuend->size))
    {
      free_array (subtrahend);
      return &the_null_array;
    }
  svt = alist_sort (subtrahend);
  difference = ALLOC_ARRAY (msize);
  for (source = minuend->item, dest = difference->item, i = (int)msize; i--; source++)
    {
      l = 0;
      o = (h = size - 1) >> 1;
      if ((source->type == T_OBJECT) && (source->u.ob->flags & O_DESTRUCTED))
        {
          free_object (source->u.ob, "subtract_array");
          *source = const0;
        }
      else if ((source->type == T_STRING)
               && !(source->subtype == STRING_SHARED))
        {
          svalue_t stmp = { .type = T_STRING, STRING_SHARED };

          if (!(stmp.u.shared_string = findstring(SVALUE_STRPTR(source), NULL)))
            {
              assign_svalue_no_free (dest++, source);
              continue;
            }
          while ((d = alist_cmp (&stmp, svt + o)))
            {
              if (d < 0)
                h = o - 1;
              else
                l = o + 1;
              if (l > h)
                {
                  assign_svalue_no_free (dest++, source);
                  break;
                }
              o = (l + h) >> 1;
            }
          continue;
        }

      while ((d = alist_cmp (source, svt + o)))
        {
          if (d < 0)
            h = o - 1;
          else
            l = o + 1;
          if (l > h)
            {
              assign_svalue_no_free (dest++, source);
              break;
            }
          o = (l + h) >> 1;
        }

    }
  i = size;
  while (i--)
    free_svalue (svt + i, "subtract_array");
  FREE ((char *) svt);
  if (subtrahend != &the_null_array)
    {
      if (subtrahend->ref > 1)
        {
          subtrahend->ref--;
        }
      else
        {
#ifdef ARRAY_STATS
          num_arrays--;
          total_array_size -= sizeof (array_t) + sizeof (svalue_t) * (size - 1);
#endif
          FREE ((char *) subtrahend);
        }
    }
  free_array (minuend);
  msize = dest - difference->item;
  if (!msize)
    {
      FREE ((char *) difference);
      return &the_null_array;
    }
  difference = RESIZE_ARRAY (difference, msize);
  difference->size = (unsigned short)msize;
  difference->ref = 1;
#ifdef ARRAY_STATS
  total_array_size += sizeof (array_t) + sizeof (svalue_t[1]) * (msize - 1);
  num_arrays++;
#endif
  return difference;
}

array_t *
intersect_array (array_t * a1, array_t * a2)
{
  array_t *a3;
  int d, l, j, i, a1s = a1->size, a2s = a2->size, flag;
  svalue_t *svt_1, *ntab, *sv_tab, *sv_ptr, val, *tmp;
  int curix, parix, child1, child2;

  if (!a1s || !a2s)
    {
      free_array (a1);
      free_array (a2);
      return &the_null_array;
    }

  svt_1 = alist_sort (a1);
  if ((flag = (a2->ref > 1)))
    {
      sv_tab = CALLOCATE (a2s, svalue_t, TAG_TEMPORARY, "intersect_array: sv2_tab");
      sv_ptr = a2->item;
      for (j = 0; j < a2s; j++)
        {
          if (((tmp = (sv_ptr + j))->type == T_OBJECT)
              && (tmp->u.ob->flags & O_DESTRUCTED))
            {
              free_object (tmp->u.ob, "intersect_array");
              sv_tab[j] = *tmp = const0;
            }
          else if ((tmp->type == T_STRING)
                   && !(tmp->subtype == STRING_SHARED))
            {
              SET_SVALUE_SHARED_STRING ((tmp = sv_tab + j),
                                        make_shared_string (SVALUE_STRPTR(tmp), NULL));
            }
          else
            assign_svalue_no_free (sv_tab + j, tmp);

          if ((curix = j))
            {
              val = *tmp;

              do
                {
                  parix = (curix - 1) >> 1;
                  if (alist_cmp (sv_tab + parix, sv_tab + curix) > 0)
                    {
                      sv_tab[curix] = sv_tab[parix];
                      sv_tab[parix] = val;
                    }
                }
              while ((curix = parix));
            }
        }
    }
  else
    {
      char *str;

      sv_tab = a2->item;
      for (j = 0; j < a2s; j++)
        {
          if (((tmp = (sv_tab + j))->type == T_OBJECT)
              && (tmp->u.ob->flags & O_DESTRUCTED))
            {
              free_object (tmp->u.ob, "alist_sort");
              *tmp = const0;
            }
          else if ((tmp->type == T_STRING)
                   && !(tmp->subtype == STRING_SHARED))
            {
              str = make_shared_string(SVALUE_STRPTR(tmp), NULL);
              free_string_svalue (tmp);
              SET_SVALUE_SHARED_STRING (tmp, str);
            }

          if ((curix = j))
            {
              val = *tmp;

              do
                {
                  parix = (curix - 1) >> 1;
                  if (alist_cmp (sv_tab + parix, sv_tab + curix) > 0)
                    {
                      sv_tab[curix] = sv_tab[parix];
                      sv_tab[parix] = val;
                    }
                }
              while ((curix = parix));
            }
        }
    }

  a3 = ALLOC_ARRAY (a2s);
  ntab = a3->item;

  l = i = 0;

  for (j = 0; j < a2s; j++)
    {
      val = sv_tab[0];

      while ((d = alist_cmp (&val, &svt_1[i])) > 0)
        {
          if (++i >= a1s)
            goto settle_business;
        }

      if (!d)
        {
          ntab[l++] = val;
        }
      else
        {
          free_svalue (&val, "intersect_array");
        }

      for (curix = 0;;)
        {
          child1 = (curix << 1) + 1;
          child2 = child1 + 1;

          if (child2 < a2s && sv_tab[child2].type != T_INVALID &&
              (sv_tab[child1].type == T_INVALID ||
               alist_cmp (sv_tab + child1, sv_tab + child2) > 0))
            {
              child1 = child2;
            }

          if (child1 < a2s && sv_tab[child1].type != T_INVALID)
            {
              sv_tab[curix] = sv_tab[child1];
              curix = child1;
            }
          else
            break;
        }

      sv_tab[curix].type = T_INVALID;
    }

settle_business:

  curix = a2s;
  while (curix--)
    {
      if (sv_tab[curix].type != T_INVALID)
        free_svalue (sv_tab + curix, "intersect_array:2");
    }

  i = a1s;
  while (i--)
    free_svalue (svt_1 + i, "intersect_array");
  FREE ((char *) svt_1);

  if (a1->ref > 1)
    a1->ref--;
  else
    {
#ifdef ARRAY_STATS
      num_arrays--;
      total_array_size -= sizeof (array_t) + sizeof (svalue_t) * (a1s - 1);
#endif
      FREE ((char *) a1);
    }

  if (flag)
    {
      a2->ref--;
      FREE ((char *) sv_tab);
    }
  else
    {
#ifdef ARRAY_STATS
      num_arrays--;
      total_array_size -= sizeof (array_t) + sizeof (svalue_t) * (a2s - 1);
#endif
      FREE ((char *) a2);
    }
  a3 = RESIZE_ARRAY (a3, l);
  a3->ref = 1;
  a3->size = (unsigned short)l;
#ifdef ARRAY_STATS
  total_array_size += sizeof (array_t) + (l - 1) * sizeof (svalue_t);
  num_arrays++;
#endif
  return a3;
}
