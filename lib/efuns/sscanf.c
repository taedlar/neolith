#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "regexp.h"
#include "sscanf.h"

#ifdef HAVE_STRTOD
#include <stdlib.h>
#else
double strtod (const char *, char **);
#endif

#define SSCANF_ASSIGN_SVALUE_STRING(S) \
        arg->type = T_STRING; \
        arg->u.string = S; \
        arg->subtype = STRING_MALLOC; \
        arg--; \
        num_arg--

#define SSCANF_ASSIGN_SVALUE_NUMBER(N) \
        arg->type = T_NUMBER; \
        arg->subtype = 0; \
        arg->u.number = N; \
        arg--; \
        num_arg--

#define SSCANF_ASSIGN_SVALUE(T,U,V) \
        arg->type = T; \
        arg->U = V; \
        arg--; \
        num_arg--

/**
 * @brief The internal sscanf function.
 */
int inter_sscanf (svalue_t * arg, svalue_t * s0, svalue_t * s1, int num_arg) {

  char *fmt;            /* Format description */
  char *in_string;      /* The string to be parsed. */
  int number_of_matches;
  int skipme;           /* Encountered a '*' ? */
  int base = 10;
  int num;
  char *match, old_char;
  register char *tmp;

  /*
   * First get the string to be parsed.
   */
  CHECK_TYPES (s0, T_STRING, 1, F_SSCANF);
  in_string = s0->u.string;

  /*
   * Now get the format description.
   */
  CHECK_TYPES (s1, T_STRING, 2, F_SSCANF);
  fmt = s1->u.string;

  /*
   * Loop for every % or substring in the format.
   */
  for (number_of_matches = 0; num_arg >= 0; number_of_matches++)
    {
      while (*fmt)
        {
          if (*fmt == '%')
            {
              if (*++fmt == '%')
                {
                  if (*in_string++ != '%')
                    return number_of_matches;
                  fmt++;
                  continue;
                }
              if (!*fmt)
                error ("*Format string end in '%%' in sscanf()");
              break;
            }
          if (*fmt++ != *in_string++)
            return number_of_matches;
        }

      if (!*fmt)
        {
          /*
           * We have reached the end of the format string.  If there are
           * any chars left in the in_string, then we put them in the
           * last variable (if any).
           */
          if (*in_string && num_arg)
            {
              number_of_matches++;
              SSCANF_ASSIGN_SVALUE_STRING (string_copy (in_string, "sscanf"));
            }
          break;
        }
      DEBUG_CHECK (fmt[-1] != '%', "In sscanf, should be a %% now!\n");

      if ((skipme = (*fmt == '*')))
        fmt++;
      else if (num_arg < 1 && *fmt != '%')
        {
          /*
           * Hmm ... maybe we should return number_of_matches here instead
           * of an error
           */
          error ("*Too few arguments to sscanf()");
        }

      switch (*fmt++)
        {
        case 'x':
          base = 16;
          /* fallthrough */
        case 'd':
          {
            tmp = in_string;
            num = (int) strtol (in_string, &in_string, base);
            if (tmp == in_string)
              return number_of_matches;
            if (!skipme)
              {
                SSCANF_ASSIGN_SVALUE_NUMBER (num);
              }
            base = 10;
            continue;
          }
        case 'f':
          {
            double tmp_num;

            tmp = in_string;
            tmp_num = strtod (in_string, &in_string);
            if (tmp == in_string)
              return number_of_matches;
            if (!skipme)
              {
                SSCANF_ASSIGN_SVALUE (T_REAL, u.real, tmp_num);
              }
            continue;
          }
        case '(':
          {
            struct regexp *reg;

            tmp = fmt;		/* 1 after the ( */
            num = 1;
            while (1)
              {
                switch (*tmp)
                  {
                  case '\\':
                    if (*++tmp)
                      {
                        tmp++;
                        continue;
                      }
                    /* fall through */
                  case '\0':
                    error ("*Bad regexp format: '%%%s' in sscanf format string", fmt);
                  case '(':
                    num++;
                    /* fall through */
                  default:
                    tmp++;
                    continue;
                  case ')':
                    if (!--num)
                      break;
                    tmp++;
                    continue;
                  }
                {
                  int n = tmp - fmt;
                  char *buf = (char *) DXALLOC (n + 1, TAG_TEMPORARY, "sscanf regexp");
                  memcpy (buf, fmt, n);
                  buf[n] = 0;
                  regexp_user = EFUN_REGEXP;
                  reg = regcomp ((unsigned char *) buf, 0);
                  FREE (buf);
                  if (!reg)
                    error (regexp_error);
                  if (!regexec (reg, in_string) || (in_string != reg->startp[0]))
                    return number_of_matches;
                  if (!skipme)
                    {
                      n = *reg->endp - in_string;
                      buf = new_string (n, "sscanf regexp return");
                      memcpy (buf, in_string, n);
                      buf[n] = 0;
                      SSCANF_ASSIGN_SVALUE_STRING (buf);
                    }
                  in_string = *reg->endp;
                  FREE ((char *) reg);
                  fmt = ++tmp;
                  break;
                }
              }
            continue;
          }
        case 's':
          break;
        default:
          error ("*Bad type : '%%%c' in sscanf() format string.", fmt[-1]);
        }

      /*
       * Now we have the string case.
       */

      /*
       * First case: There were no extra characters to match. Then this is
       * the last match.
       */
      if (!*fmt)
        {
          number_of_matches++;
          if (!skipme)
            {
              SSCANF_ASSIGN_SVALUE_STRING (string_copy (in_string, "sscanf"));
            }
          break;
        }
      /*
       * If the next char in the format string is a '%' then we have to do
       * some special checks. Only %d, %f, %x, %(regexp) and %% are allowed
       * after a %s
       */
      if (*fmt++ == '%')
        {
          int skipme2;

          tmp = in_string;
          if ((skipme2 = (*fmt == '*')))
            fmt++;
          if (num_arg < (!skipme + !skipme2) && *fmt != '%')
            error ("*Too few arguments to sscanf().");

          number_of_matches++;

          switch (*fmt++)
            {
            case 's':
              error ("*Illegal to have 2 adjacent %%s's in format string in sscanf()");
            case 'x':
              do
                {
                  while (*tmp && (*tmp != '0'))
                    tmp++;
                  if (*tmp == '0')
                    {
                      if ((tmp[1] == 'x' || tmp[1] == 'X') &&
                          isxdigit (tmp[2]))
                        break;
                      tmp += 2;
                    }
                }
              while (*tmp);
              break;
            case 'd':
              while (*tmp && !isdigit (*tmp))
                tmp++;
              break;
            case 'f':
              while (*tmp && !isdigit (*tmp) && (*tmp != '.' || !isdigit (tmp[1])))
                tmp++;
              break;
            case '%':
              while (*tmp && (*tmp != '%'))
                tmp++;
              break;
            case '(':
              {
                struct regexp *reg;

                tmp = fmt;
                num = 1;
                while (1)
                  {
                    switch (*tmp)
                      {
                      case '\\':
                        if (*++tmp)
                          {
                            tmp++;
                            continue;
                          }
                        /* fall through */
                      case '\0':
                        error ("*Bad regexp format : '%%%s' in sscanf format string.", fmt);
                      case '(':
                        num++;
                        /* fall through */
                      default:
                        tmp++;
                        continue;

                      case ')':
                        if (!--num)
                          break;
                        tmp++;
                        continue;
                      }
                    {
                      int n = tmp - fmt;
                      char *buf = (char *) DXALLOC (n + 1, TAG_TEMPORARY, "sscanf regexp");
                      memcpy (buf, fmt, n);
                      buf[n] = 0;
                      regexp_user = EFUN_REGEXP;
                      reg = regcomp ((unsigned char *) buf, 0);
                      FREE (buf);
                      if (!reg)
                        error (regexp_error);
                      if (!regexec (reg, in_string))
                        {
                          if (!skipme)
                            {
                              SSCANF_ASSIGN_SVALUE_STRING (string_copy (in_string, "sscanf"));
                            }
                          FREE ((char *) reg);
                          return number_of_matches;
                        }
                      else
                        {
                          if (!skipme)
                            {
                              match = new_string (num = (*reg->startp - in_string), "inter_sscanf");
                              memcpy (match, in_string, num);
                              match[num] = 0;
                              SSCANF_ASSIGN_SVALUE_STRING (match);
                            }
                          in_string = *reg->endp;
                          if (!skipme2)
                            {
                              match = new_string (num = (*reg->endp - *reg->startp), "inter_sscanf");
                              memcpy (match, *reg->startp, num);
                              match[num] = 0;
                              SSCANF_ASSIGN_SVALUE_STRING (match);
                            }
                          FREE ((char *) reg);
                        }
                      fmt = ++tmp;
                      break;
                    }
                  }
                continue;
              }

            case 0:
              error ("*Format string can't end in '%%'.");
            default:
              error ("*Bad type : '%%%c' in sscanf() format string.", fmt[-1]);
            }

          if (!skipme)
            {
              match = new_string (num = (tmp - in_string), "inter_sscanf");
              memcpy (match, in_string, num);
              match[num] = 0;
              SSCANF_ASSIGN_SVALUE_STRING (match);
            }
          if (!*(in_string = tmp))
            return number_of_matches;
          switch (fmt[-1])
            {
            case 'x':
              base = 16;
              /* fall through */
            case 'd':
              {
                num = (int) strtol (in_string, &in_string, base);
                /* We already knew it would be matched - Sym */
                if (!skipme2)
                  {
                    SSCANF_ASSIGN_SVALUE_NUMBER (num);
                  }
                base = 10;
                continue;
              }
            case 'f':
              {
                double tmp_num = strtod (in_string, &in_string);
                if (!skipme2)
                  {
                    SSCANF_ASSIGN_SVALUE (T_REAL, u.real, tmp_num);
                  }
                continue;
              }
            case '%':
              in_string++;
              continue;		/* on the big for loop */
            }
        }
      if ((tmp = strchr (fmt, '%')) != NULL)
        num = tmp - fmt + 1;
      else
        {
          tmp = fmt + (num = strlen (fmt));
          num++;
        }

      old_char = *--fmt;
      match = in_string;

      /* This loop would be even faster if it used replace_string's skiptable
         algorithm.  Maybe that algorithm should be lifted so it can be
         used in strsrch as well has here, etc? */
      while (*in_string)
        {
          if ((*in_string == old_char) && !strncmp (in_string, fmt, num))
            {
              /*
               * Found a match !
               */
              if (!skipme)
                {
                  char *newmatch;

                  newmatch = new_string (skipme = (in_string - match), "inter_sscanf");
                  memcpy (newmatch, match, skipme);
                  newmatch[skipme] = 0;
                  SSCANF_ASSIGN_SVALUE_STRING (newmatch);
                }
              in_string += num;
              fmt = tmp;	/* advance fmt to next % */
              break;
            }
          in_string++;
        }
      if (fmt == tmp)		/* If match, then do continue. */
        continue;

      /*
       * No match was found. Then we stop here, and return the result so
       * far !
       */
      break;
    }
  return number_of_matches;
}

#ifndef HAVE_STRTOD
double strtod (const char *nptr, char **endptr)
{
  register const char *s = nptr;
  register double acc;
  register int neg, c, any, div;

  div = 1;
  neg = 0;
  /*
   * Skip white space and pick up leading +/- sign if any.
   */
  do
    {
      c = *s++;
    }
  while (isspace (c));
  if (c == '-')
    {
      neg = 1;
      c = *s++;
    }
  else if (c == '+')
    c = *s++;

  for (acc = 0, any = 0;; c = *s++)
    {
      if (isdigit (c))
        c -= '0';
      else if ((div == 1) && (c == '.'))
        {
          div = 10;
          continue;
        }
      else
        break;
      if (div == 1)
        {
          acc *= (double) 10;
          acc += (double) c;
        }
      else
        {
          acc += (double) c / (double) div;
          div *= 10;
        }
      any = 1;
    }

  if (neg)
    acc = -acc;

  if (endptr != 0)
    *endptr = any ? s - 1 : (char *) nptr;

  return acc;
}
#endif /* !HAVE_STRTOD */

#ifdef F_SSCANF
void f_sscanf () {
  svalue_t *fp;
  int i;
  int num_arg;

  /*
   * get number of lvalue args
   */
  num_arg = EXTRACT_UCHAR (pc);
  pc++;

  /*
   * allocate stack frame for rvalues and return value (number of matches);
   * perform some stack manipulation; note: source and template strings are
   * already on the stack by this time
   */
  fp = sp;
  sp += num_arg + 1;
  *sp = *(fp--);        /* move format description to top of stack */
  *(sp - 1) = *(fp);    /* move source string just below the format desc. */
  fp->type = T_NUMBER;  /* this svalue isn't invalidated below, and
                         * if we don't change it to something safe,
                         * it will get freed twice if an error occurs */
  /*
   * prep area for rvalues
   */
  for (i = 1; i <= num_arg; i++)
    fp[i].type = T_INVALID;

  /*
   * do it...
   */
  i = inter_sscanf (sp - 2, sp - 1, sp, num_arg);

  /*
   * remove source & template strings from top of stack
   */
  pop_2_elems ();

  /*
   * save number of matches on stack
   */
  fp->u.number = i;
  fp->subtype = 0;
}
#endif
