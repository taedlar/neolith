#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "src/interpret.h"
#include "rc.h"
#include "crc32.h"
#include "lpc/array.h"
#include "lpc/buffer.h"
#include "lpc/functional.h"
#include "lpc/operator.h"
#include "regexp.h"
#include "sprintf.h"

#ifdef F_CAPITALIZE
void f_capitalize (void) {
  if (islower (SVALUE_STRPTR(sp)[0]))
    {
      unlink_string_svalue (sp);
      sp->u.malloc_string[0] += 'A' - 'a';
    }
}
#endif


#ifdef F_C_STR
void f_c_str (void) {
  const char *src;
  size_t span_len, c_len;
  malloc_str_t result;

  src = SVALUE_STRPTR(sp);
  span_len = SVALUE_STRLEN(sp);
  c_len = strlen(src);
  if (c_len > span_len)
    c_len = span_len;

  result = new_string(c_len, "f_c_str");
  if (c_len)
    {
      memcpy(result, src, c_len);
    }
  result[c_len] = '\0';

  free_string_svalue(sp);
  SET_SVALUE_MALLOC_STRING(sp, result);
}
#endif


#ifdef F_LOWER_CASE
void f_lower_case (void) {
  register const char *str;

  str = SVALUE_STRPTR(sp);
  /* find first upper case letter, if any */
  for (; *str; str++)
    {
      if (isupper (*str))
        {
          char* p;
          size_t len = str - SVALUE_STRPTR(sp);
          unlink_string_svalue (sp);
          p = sp->u.malloc_string + len;
          *p += 'a' - 'A';
          for (p++; *p; p++)
            {
              if (isupper (*p))
                *p += 'a' - 'A';
            }
          return;
        }
    }
}
#endif


#ifdef F_UPPER_CASE
void f_upper_case (void) {
  register const char *str;

  str = SVALUE_STRPTR(sp);
  /* find first upper case letter, if any */
  for (; *str; str++)
    {
      if (islower (*str))
        {
          char* p;
          size_t len = str - SVALUE_STRPTR(sp);
          unlink_string_svalue (sp);
          p = sp->u.malloc_string + len;
          *p -= 'a' - 'A';
          for (p++; *p; p++)
            {
              if (islower (*p))
                *p -= 'a' - 'A';
            }
          return;
        }
    }
}
#endif


#ifdef F_STRWRAP
void f_strwrap (void) {
  size_t indent = 0, width = 0;

  if (st_num_arg == 3)
    indent = (size_t)(sp--)->u.number;

  width = (size_t)(sp--)->u.number;

  /* TODO: wrap the string in specified width and indent */
  opt_trace(TT_EVAL|3, "indent=%zu, width=%zu: strwrap() not implemented yet.\n", indent, width);
}
#endif


#ifdef F_REPEAT_STRING
void f_repeat_string (void) {
  const char *str;
  size_t repeat, len;
  char *ret, *p;
  size_t i;

  repeat = (size_t)((sp--)->u.number);
  if (repeat <= 0)
    {
      free_string_svalue (sp);
      SET_SVALUE_CONSTANT_STRING(sp, "");
    }
  else if (repeat != 1)
    {
      str = SVALUE_STRPTR(sp);
      len = SVALUE_STRLEN (sp);
      if (len * repeat > (size_t)CONFIG_INT (__MAX_STRING_LENGTH__))
        error ("repeat_string: String too large.\n");
//      repeat = CONFIG_INT(__MAX_STRING_LENGTH__) / len;
      p = ret = new_string (len * repeat, "f_repeat_string");
      for (i = 0; i < repeat; i++)
        {
          memcpy (p, str, len);
          p += len;
        }
      *p = 0;
      free_string_svalue (sp);
      SET_SVALUE_MALLOC_STRING(sp, ret);
    }
}
#endif


#ifdef F_CRYPT
#define SALT_LEN	8

#ifndef HAVE_UNISTD_H
char *crypt (const char *key, const char *salt);
#endif

void f_crypt (void) {
  const char* p;
  char *res, salt[SALT_LEN + 1];
  char *choice = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ./";

  if (sp->type == T_STRING && SVALUE_STRLEN (sp) >= 2)
    {
      p = SVALUE_STRPTR(sp);
    }
  else
    {
      int i;

      for (i = 0; i < SALT_LEN; i++)
        salt[i] = choice[rand () % strlen (choice)];

      salt[SALT_LEN] = 0;
      p = salt;
    }

  res = string_copy (crypt (SVALUE_STRPTR(sp - 1), p), "f_crypt");
  pop_stack ();
  free_string_svalue (sp);
  SET_SVALUE_MALLOC_STRING(sp, res);
}
#endif

#ifdef F_OLDCRYPT
void f_oldcrypt (void) {
  char *res, salt[3];
  char *choice = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ./";

  if (sp->type == T_STRING && SVALUE_STRLEN (sp) >= 2)
    {
      salt[0] = SVALUE_STRPTR(sp)[0];
      salt[1] = SVALUE_STRPTR(sp)[1];
      free_string_svalue (sp--);
    }
  else
    {
      salt[0] = choice[rand () % strlen (choice)];
      salt[1] = choice[rand () % strlen (choice)];
      pop_stack ();
    }
  salt[2] = 0;
  res = string_copy (crypt (SVALUE_STRPTR(sp), salt), "f_crypt");
  free_string_svalue (sp);
  SET_SVALUE_MALLOC_STRING(sp, res);
}
#endif

#ifdef F_CRC32
void f_crc32 (void) {
  size_t len;
  unsigned char *buf;
  uint32_t crc;

  if (sp->type == T_STRING)
    {
      len = SVALUE_STRLEN (sp);
      buf = (unsigned char *) SVALUE_STRPTR(sp);
    }
  else if (sp->type == T_BUFFER)
    {
      len = sp->u.buf->size;
      buf = sp->u.buf->item;
    }
  else
    {
      bad_argument (sp, T_STRING | T_BUFFER, 1, F_CRC32);
      return;
    }
  crc = compute_crc32 (buf, len);
  free_svalue (sp, "f_crc32");
  put_number (crc);
}
#endif


#ifdef F_IMPLODE
void f_implode (void) {
  array_t *arr;
  int flag;
  svalue_t *args;

  if (st_num_arg == 3)
    {
      args = (sp - 2);
      if (args[1].type == T_STRING)
        error ("Third argument to implode() is illegal with implode(array, string)\n");
      flag = 1;
    }
  else
    {
      args = (sp - 1);
      flag = 0;
    }

  arr = args->u.arr;
  check_for_destr (arr);

  if (args[1].type == T_STRING)
    {
      /* st_num_arg == 2 here */
      char *str;

      str = implode_string (arr, SVALUE_STRPTR(sp), SVALUE_STRLEN (sp));
      free_string_svalue (sp--);
      free_array (arr);
      put_malloced_string (str);
    }
  else
    {
      funptr_t *funp = args[1].u.fp;

      /* this pulls the extra arg off the stack if it exists */
      implode_array (funp, arr, args, flag);
      sp--;
      free_funp (funp);
      free_array (arr);
    }
}
#endif


#ifdef F_EXPLODE
void f_explode (void) {
  array_t *vec;

  vec = explode_string (SVALUE_STRPTR(sp - 1), SVALUE_STRLEN (sp - 1),
                        SVALUE_STRPTR(sp), SVALUE_STRLEN (sp));
  free_string_svalue (sp--);
  free_string_svalue (sp);
  put_array (vec);
}
#endif


#ifdef F_REG_ASSOC
/*
 * write(sprintf("%O", reg_assoc("testhahatest", ({ "haha", "te" }),
 *              ({ 2,3 }), 4)));

 * --------
 * ({
 *   ({ "", "te", "st", "haha", "", "te", "st" }),
 *   ({  4,    3,    4,      2,  4,    3,   4  })
 * })
 *
 */
static array_t *
reg_assoc (const char *str, array_t * pat, array_t * tok, svalue_t * def)
{
  int i, size;
  const char *tmp;
  array_t *ret;

  regexp_user = EFUN_REGEXP;
  if ((size = pat->size) != tok->size)
    error ("Pattern and token array sizes must be identical.\n");

  for (i = 0; i < size; i++)
    {
      if (!(pat->item[i].type == T_STRING))
        error ("Non-string found in pattern array.\n");
    }

  ret = allocate_empty_array (2);

  if (size)
    {
      struct regexp **rgpp;
      struct reg_match
      {
        int tok_i;
        const char *begin, *end;
        struct reg_match *next;
      }
       *rmp = (struct reg_match *) 0, *rmph = (struct reg_match *) 0;
      int num_match = 0;
      svalue_t *sv1, *sv2, *sv;
      int index;
      struct regexp *tmpreg;
      const char *laststart, *currstart;

      rgpp =
        CALLOCATE (size, struct regexp *, TAG_TEMPORARY, "reg_assoc : rgpp");
      for (i = 0; i < size; i++)
        {
          if (!
              (rgpp[i] =
               regcomp ((unsigned char *) SVALUE_STRPTR(&pat->item[i]), 0)))
            {
              while (i--)
                FREE ((char *) rgpp[i]);
              FREE ((char *) rgpp);
              free_empty_array (ret);
              error (regexp_error);
            }
        }

      tmp = str;
      while (*tmp)
        {

          /* Sigh - need a kludge here - Randor */
          /* In the future I may alter regexp.c to include branch info */
          /* so as to minimize checks here - Randor 5/30/94 */

          laststart = 0;
          index = -1;

          for (i = 0; i < size; i++)
            {
              if (regexec (tmpreg = rgpp[i], tmp))
                {
                  currstart = tmpreg->startp[0];
                  if (tmp == currstart)
                    {
                      index = i;
                      break;
                    }
                  if (!laststart || currstart < laststart)
                    {
                      laststart = currstart;
                      index = i;
                    }
                }
            }

          if (index >= 0)
            {
              num_match++;
              if (rmp)
                {
                  rmp->next = ALLOCATE (struct reg_match, TAG_TEMPORARY, "reg_assoc : rmp->next");
                  rmp = rmp->next;
                }
              else
                rmph = rmp = ALLOCATE (struct reg_match, TAG_TEMPORARY, "reg_assoc : rmp");
              tmpreg = rgpp[index];
              rmp->begin = tmpreg->startp[0];
              rmp->end = tmp = tmpreg->endp[0];
              rmp->tok_i = index;
              rmp->next = (struct reg_match *) 0;
            }
          else
            break;

          /* The following is from regexplode, to prevent i guess infinite */
          /* loops on "" patterns - Randor 5/29/94 */
          if (rmp->begin == tmp && (!*++tmp))
            break;
        }

      sv = ret->item;
      sv->type = T_ARRAY;
      sv1 = (sv->u.arr = allocate_empty_array (2 * num_match + 1))->item;

      sv++;
      sv->type = T_ARRAY;
      sv2 = (sv->u.arr = allocate_empty_array (2 * num_match + 1))->item;

      rmp = rmph;

      tmp = str;

      while (num_match--)
        {
          char *svtmp;
          size_t length;

          length = rmp->begin - tmp;
          SET_SVALUE_MALLOC_STRING (sv1, svtmp = new_string (length, "reg_assoc : sv1"));
          strncpy (svtmp, tmp, length);
          svtmp[length] = 0;
          sv1++;
          assign_svalue_no_free (sv2++, def);
          tmp += length;
          length = rmp->end - rmp->begin;
          SET_SVALUE_MALLOC_STRING (sv1, svtmp = new_string (length, "reg_assoc : sv1"));
          strncpy (svtmp, tmp, length);
          svtmp[length] = 0;
          sv1++;
          assign_svalue_no_free (sv2++, &tok->item[rmp->tok_i]);
          tmp += length;
          rmp = rmp->next;
        }
      SET_SVALUE_MALLOC_STRING (sv1, string_copy (tmp, "reg_assoc"));
      assign_svalue_no_free (sv2, def);
      for (i = 0; i < size; i++)
        FREE ((char *) rgpp[i]);
      FREE ((char *) rgpp);

      while ((rmp = rmph))
        {
          rmph = rmp->next;
          FREE ((char *) rmp);
        }
      return ret;
    }
  else
    {				/* Default match */
      svalue_t *temp;
      svalue_t *sv;

      (sv = ret->item)->type = T_ARRAY;
      temp = (sv->u.arr = allocate_empty_array (1))->item;
      SET_SVALUE_MALLOC_STRING (temp, string_copy (str, "reg_assoc"));
      sv = &ret->item[1];
      sv->type = T_ARRAY;
      assign_svalue_no_free ((sv->u.arr = allocate_empty_array (1))->item, def);
      return ret;
    }
}

void f_reg_assoc (void) {
  svalue_t *arg;
  array_t *vec;

  arg = sp - st_num_arg + 1;

  if (!(arg[2].type == T_ARRAY))
    error ("Bad argument 3 to reg_assoc()\n");

  vec =
    reg_assoc (SVALUE_STRPTR(&arg[0]), arg[1].u.arr, arg[2].u.arr,
               st_num_arg > 3 ? &arg[3] : &const0);

  if (st_num_arg == 4)
    pop_3_elems ();
  else
    pop_2_elems ();
  free_string_svalue (sp);
  sp->type = T_ARRAY;
  sp->u.arr = vec;
}
#endif


#ifdef F_REGEXP
static int
match_single_regexp (const char *str, const char *pattern)
{
  struct regexp *reg;
  int ret;

  regexp_user = EFUN_REGEXP;
  reg = regcomp ((unsigned char *) pattern, 0);
  if (!reg)
    error (regexp_error);
  ret = regexec (reg, str);
  FREE ((char *) reg);
  return ret;
}

static array_t *
match_regexp (array_t * v, const char *pattern, int flag)
{
  struct regexp *reg;
  char *res;
  int num_match, size, match = !(flag & 2);
  array_t *ret;
  svalue_t *sv1, *sv2;

  regexp_user = EFUN_REGEXP;
  if (!(size = v->size))
    return &the_null_array;
  reg = regcomp ((unsigned char *) pattern, 0);
  if (!reg)
    error (regexp_error);
  res = (char *) DMALLOC (size, TAG_TEMPORARY, "match_regexp: res");
  sv1 = v->item + size;
  num_match = 0;
  while (size--)
    {
      if (!((--sv1)->type == T_STRING)
          || (regexec (reg, SVALUE_STRPTR(sv1)) != match))
        {
          res[size] = 0;
        }
      else
        {
          res[size] = 1;
          num_match++;
        }
    }

  flag &= 1;
  ret = allocate_empty_array (num_match << flag);
  sv2 = ret->item + (num_match << flag);
  size = v->size;
  while (size--)
    {
      if (res[size])
        {
          if (flag)
            {
              (--sv2)->type = T_NUMBER;
              sv2->u.number = size + 1;
            }
          (--sv2)->type = T_STRING;
          sv1 = v->item + size;
          *sv2 = *sv1;
          if (sv1->subtype == STRING_MALLOC)
            {
              INC_COUNTED_REF (sv1->u.malloc_string);
              ADD_STRING (MSTR_SIZE (sv1->u.malloc_string));
            }
          else if (sv1->subtype == STRING_SHARED)
            {
              INC_COUNTED_REF (sv1->u.shared_string);
              ADD_STRING (MSTR_SIZE (sv1->u.shared_string));
            }
          if (!--num_match)
            break;
        }
    }
  FREE (res);
  FREE ((char *) reg);
  return ret;
}

void f_regexp (void) {
  array_t *v;
  int flag;

  if (st_num_arg > 2)
    {
      if (!(sp->type == T_NUMBER))
        error ("Bad argument 3 to regexp()\n");
      if (sp[-2].type == T_STRING)
        error ("3rd argument illegal for regexp(string, string)\n");
      flag = (int)(sp--)->u.number;
    }
  else
    flag = 0;
  if (sp[-1].type == T_STRING)
    {
      flag = match_single_regexp (SVALUE_STRPTR(sp - 1), SVALUE_STRPTR(sp));
      free_string_svalue (sp--);
      free_string_svalue (sp);
      put_number (flag);
    }
  else
    {
      v = match_regexp ((sp - 1)->u.arr, SVALUE_STRPTR(sp), flag);
      free_string_svalue (sp--);
      free_array (sp->u.arr);
      sp->u.arr = v;
    }
}
#endif


/* This is an enhancement to the f_replace_string() in efuns_main.c of
   MudOS v21.  When the search pattern has more than one character,
   this version of f_replace_string() uses a skip table to more efficiently
   search the file for the search pattern (the basic idea is to avoid
   strings comparisons where possible).  This version is anywhere from
   15% to 40% faster than the old version depending on the size of the
   string to be searched and the length of the search string (and depending
   on the relative frequency with which the letters in the search string
   appear in the string to be searched).

   Note: this version should behave identically to the old version (except
   for runtime).  When the search pattern is only one character long, the
   old algorithm is used.  The new algorithm is actually about 10% slower
   than the old one when the search string is only one character long.

   This enhancement to f_replace_string() was written by John Garnett
   (aka Truilkan) on 1995/04/29.  I believe the original replace_string()
   was written by Dave Richards (Cygnus).

   I didn't come up with the idea of this algorithm (learned it in
   a university programming course way back when).  For those interested
   in the workings of the algorithm, you can probably find it in a book on
   string processing algorithms.  Its also fairly easy to figure out the
   algorithm by tracing through it for a small example.
*/

#ifdef F_REPLACE_STRING
/**
 * Syntax for replace_string is now:
    string replace_string(src, pat, rep);   // or
    string replace_string(src, pat, rep, max);  // or
    string replace_string(src, pat, rep, first, last);

 * The 4th/5th args are optional (to retain backward compatibility).
 * - src, pat, and rep are all strings.
 * - max is an integer. It will replace all occurances up to max
 *   matches (starting as 1 as the first), with a value of 0 meaning
 *   'replace all')
 * - first and last are just a range to replace between, with
 *   the following constraints
 *    first < 1: change all from start
 *    last == 0 || last > max matches:    change all to end
 *    first > last: return unmodified array.
 *    (i.e, with 4 args, it's like calling it with:
 *      replace_string(src, pat, rep, 0, max);
 *    )
 */
void f_replace_string (void) {
  size_t plen, rlen, dlen, slen, first, last, cur, j;

  const char *pattern;
  const char *replace;
  register char *src, *dst1, *dst2;
  svalue_t *arg;
  size_t skip_table[256];
  const char *slimit = NULL;
  const char *flimit = NULL;
  const char *climit = NULL;
  size_t probe = 0, skip;

  if (st_num_arg > 5)
    {
      error ("Too many args to replace_string.\n");
      pop_n_elems (st_num_arg);
      return;
    }
  arg = sp - st_num_arg + 1;
  opt_trace (TT_EVAL|3, "src ='%s'\n", SVALUE_STRPTR(arg));
  first = 0;
  last = 0;

  if (st_num_arg >= 4)
    {
      CHECK_TYPES ((arg + 3), T_NUMBER, 4, F_REPLACE_STRING);
      first = (size_t)(arg + 3)->u.number;

      if (st_num_arg == 4)
        {
          last = first;
          first = 0;
        }
      else if (st_num_arg == 5)
        {
          CHECK_TYPES ((arg + 4), T_NUMBER, 5, F_REPLACE_STRING);
          /* first was set above. */
          last = (size_t)(arg + 4)->u.number;
        }
    }

  if (!last)
    last = CONFIG_INT (__MAX_STRING_LENGTH__);

  if (first > last)
    {				/* just return it */
      pop_n_elems (st_num_arg - 1);
      return;
    }

  pattern = SVALUE_STRPTR(arg + 1);
  plen = SVALUE_STRLEN (arg + 1);
  opt_trace (TT_EVAL|3, "pattern ='%s' (%d)\n", pattern, plen);
  if (plen < 1)
    {
      pop_n_elems (st_num_arg - 1);	/* just return it */
      return;
    }

  replace = SVALUE_STRPTR(arg + 2);
  rlen = SVALUE_STRLEN (arg + 2);
  opt_trace (TT_EVAL|3, "replace ='%s' (%d)\n", replace, rlen);
  dlen = 0;
  cur = 0;

  if ((arg->subtype != STRING_MALLOC) || (rlen <= plen))
    unlink_string_svalue (arg);
  src = arg->u.malloc_string;

  if (plen > 1)
    {
      /* build skip table */
      for (j = 0; j < 256; j++)
        skip_table[j] = plen;
      for (j = 0; j < plen; j++)
        skip_table[(unsigned char) pattern[j]] = plen - j - 1;

      slen = SVALUE_STRLEN (arg);
      slimit = src + slen;
      flimit = slimit - plen + 1;
      probe = plen - 1;
    }

  if (rlen <= plen)
    {
      dst2 = dst1 = arg->u.malloc_string;

      if (plen > 1)
        {
          while (src < flimit)
            {
              if ((skip = skip_table[(unsigned char) src[probe]]))
                {
                  for (climit = dst2 + skip; dst2 < climit; *dst2++ = *src++)
                    ;
                }
              else if (memcmp (src, pattern, plen) == 0)
                {
                  cur++;
                  if ((cur >= first) && (cur <= last))
                    {
                      if (rlen)
                        {
                          memcpy (dst2, replace, rlen);
                          dst2 += rlen;
                        }
                      src += plen;
                      if (cur == last)
                        break;
                    }
                  else
                    {
                      memcpy (dst2, src, plen);
                      dst2 += plen;
                      src += plen;
                    }
                }
              else
                {
                  *dst2++ = *src++;
                }
            }
          memcpy (dst2, src, slimit - src);
          dst2 += (slimit - src);
          *dst2 = 0;
          arg->u.malloc_string = extend_string (dst1, dst2 - dst1);
        }
      else
        {
          if (rlen)
            {
              while (*src)
                {
                  if (*src == *pattern)
                    {
                      cur++;
                      if (cur < first)
                        continue;
                      *src = *replace;
                      if (cur > last)
                        break;
                    }
                  src++;
                }
            }
          else
            {			/* rlen is zero */
              while (*src)
                {
                  if (*src++ == *pattern)
                    {
                      cur++;
                      if (cur >= first)
                        {
                          dst2 = src - 1;
                          while (*src)
                            {
                              if (*src == *pattern)
                                {
                                  cur++;
                                  if (cur <= last)
                                    {
                                      src++;
                                      continue;
                                    }
                                  else
                                    {
                                      while (*src)
                                        *dst2++ = *src++;
                                      break;
                                    }
                                }
                              *dst2++ = *src++;
                            }
                          *dst2 = '\0';
                          arg->u.malloc_string = extend_string (dst1, dst2 - dst1);
                          break;
                        }
                    }
                }
            }
        }
      pop_n_elems (st_num_arg - 1);
    }
  else
    {
      dst2 = dst1 = new_string (CONFIG_INT (__MAX_STRING_LENGTH__), "f_replace_string: 2");

      if (plen > 1)
        {
          while (src < flimit)
            {
              if ((skip = skip_table[(unsigned char) src[probe]]))
                {
                  for (climit = dst2 + skip; dst2 < climit; *dst2++ = *src++)
                    ;

                }
              else if (memcmp (src, pattern, plen) == 0)
                {
                  cur++;
                  if ((cur >= first) && (cur <= last))
                    {
                      if (CONFIG_INT (__MAX_STRING_LENGTH__) - dlen <= rlen)
                        {
                          pop_n_elems (st_num_arg);
                          push_svalue (&const0u);
                          FREE_MSTR (dst1);
                          return;
                        }
                      memcpy (dst2, replace, rlen);
                      dst2 += rlen;
                      dlen += rlen;
                      src += plen;
                      if (cur == last)
                        break;
                    }
                  else
                    {
                      dlen += plen;
                      if (CONFIG_INT (__MAX_STRING_LENGTH__) - dlen <= 0)
                        {
                          pop_n_elems (st_num_arg);
                          push_svalue (&const0u);

                          FREE_MSTR (dst1);
                          return;
                        }
                      memcpy (dst2, src, plen);
                      dst2 += plen;
                      src += plen;
                    }
                }
              else
                {
                  if (CONFIG_INT (__MAX_STRING_LENGTH__) - dlen <= 1)
                    {
                      pop_n_elems (st_num_arg);
                      push_svalue (&const0u);

                      FREE_MSTR (dst1);
                      return;
                    }
                  *dst2++ = *src++;
                  dlen++;
                }
            }
          if ((ptrdiff_t)(CONFIG_INT (__MAX_STRING_LENGTH__) - dlen) <= (slimit - src))
            {
              pop_n_elems (st_num_arg);
              push_svalue (&const0u);
              FREE_MSTR (dst1);
              return;
            }
          memcpy (dst2, src, slimit - src);
          dst2 += (slimit - src);
        }
      else
        {			/* plen <= 1 */
          /* Beek: plen == 1 */
          while (*src != '\0')
            {
              if (*src == *pattern)
                {
                  cur++;
                  if (cur >= first && cur <= last)
                    {
                      if (rlen != 0)
                        {
                          if (CONFIG_INT (__MAX_STRING_LENGTH__) - dlen <=
                              rlen)
                            {
                              pop_n_elems (st_num_arg);
                              push_svalue (&const0u);
                              FREE_MSTR (dst1);
                              return;
                            }
                          strncpy (dst2, replace, rlen);
                          dst2 += rlen;
                          dlen += rlen;
                        }
                      src++;
                      continue;
                    }
                }
              if (CONFIG_INT (__MAX_STRING_LENGTH__) - dlen <= 1)
                {
                  pop_n_elems (st_num_arg);
                  push_svalue (&const0u);
                  FREE_MSTR (dst1);
                  return;
                }
              *dst2++ = *src++;
              dlen++;
            }
        }
      *dst2 = '\0';

      pop_n_elems (st_num_arg);
      /*
       * shrink block or make a copy of exact size
       */
      push_malloced_string (extend_string (dst1, dst2 - dst1));
      opt_trace (TT_EVAL|2, "returning \"%s\"", sp->type == T_STRING ? SVALUE_STRPTR(sp) : "(non-string)");
    }
}
#endif


#ifdef F_STRSRCH
/*
 * int strsrch(string big, string little, [ int flag ])
 * - search for little in big, starting at right if flag is set
 *   return int offset of little, -1 if not found
 *
 * Written 930706 by Luke Mewburn <zak@rmit.edu.au>
 * Added support for unicode character by Annihilator.
 */

void f_strsrch (void) {
  register const char *big, *little, *pos;
  wchar_t wch[2];
  char mbs[10];
  int i;
  size_t blen, llen;

  sp--;
  big = SVALUE_STRPTR(sp - 1);
  blen = SVALUE_STRLEN (sp - 1);
  if (sp->type == T_NUMBER)
    {
      /* search for single (wide) character */
      wch[0] = (wchar_t) sp->u.number;
      wch[1] = 0;
      llen = wcstombs (mbs, wch, sizeof (mbs));
      little = mbs;
      if (llen < 1)
        error ("strsrch: invalid wide character\n");
    }
  else
    {
      little = SVALUE_STRPTR(sp);
      llen = SVALUE_STRLEN (sp);
    }

  if (!llen || blen < llen)
    {
      pos = NULL;
    }
  else if (!((sp + 1)->u.number)) /* start at left */
    {
      if (!little[1])		/* 1 char srch pattern */
        pos = strchr (big, (int) little[0]);
      else
        pos = (char *) strstr (big, little);
    }
  else /* start at right */
    {				/* XXX: maybe test for -1 */
      if (!little[1])		/* 1 char srch pattern */
        pos = strrchr (big, (int) little[0]);
      else
        {
          char c = *little;

          pos = big + blen;	/* find end */
          pos -= llen;		/* find rightmost pos it _can_ be */
          do
            {
              do
                {
                  if (*pos == c)
                    break;
                }
              while (--pos >= big);
              if (*pos != c)
                {
                  pos = NULL;
                  break;
                }
              for (i = 1; little[i] && (pos[i] == little[i]); i++);	/* scan all chars */
              if (!little[i])
                break;
            }
          while (--pos >= big);
        }
    }

  if (!pos)
    i = -1;
  else
    i = (int) (pos - big);
  if (sp->type == T_STRING)
    free_string_svalue (sp);
  free_string_svalue (--sp);
  put_number (i);
}				/* strsrch */
#endif


#ifdef F_STRCMP
void f_strcmp (void) {
  int i;

  i = strcmp (SVALUE_STRPTR(sp - 1), SVALUE_STRPTR(sp));
  free_string_svalue (sp--);
  free_string_svalue (sp);
  put_number (i);
}
#endif
