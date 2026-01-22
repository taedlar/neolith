#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "rc.h"
#include "lpc/include/runtime_config.h"


#ifdef F_TEST_BIT
void
f_test_bit (void)
{
  int ind = (int)(sp--)->u.number;

  if (ind / 6 >= (int)SVALUE_STRLEN (sp))
    {
      free_string_svalue (sp);
      *sp = const0;
      return;
    }
  if (ind < 0)
    error ("Bad argument 2 (negative) to test_bit().\n");
  if ((sp->u.string[ind / 6] - ' ') & (1 << (ind % 6)))
    {
      free_string_svalue (sp);
      *sp = const1;
    }
  else
    {
      free_string_svalue (sp);
      *sp = const0;
    }
}
#endif


#ifdef F_NEXT_BIT
void
f_next_bit (void)
{
  int start = (int)(sp--)->u.number;
  size_t len = SVALUE_STRLEN (sp);
  int which, bit = 0, value;

  if (!len || start / 6 >= (int)len)
    {
      free_string_svalue (sp);
      put_number (-1);
      return;
    }
  /* Find the next bit AFTER start */
  if (start > 0)
    {
      if (start % 6 == 5)
        {
          which = (start / 6) + 1;
          value = sp->u.string[which] - ' ';
        }
      else
        {
          /* we have a partial byte to check */
          which = start / 6;
          bit = 0x3f - ((1 << ((start % 6) + 1)) - 1);
          value = (sp->u.string[which] - ' ') & bit;
        }
    }
  else
    {
      which = 0;
      value = *sp->u.string - ' ';
    }

  while (1)
    {
      if (value)
        {
          if (value & 0x07)
            {
              if (value & 0x01)
                bit = which * 6;
              else if (value & 0x02)
                bit = which * 6 + 1;
              else if (value & 0x04)
                bit = which * 6 + 2;
              break;
            }
          else if (value & 0x38)
            {
              if (value & 0x08)
                bit = which * 6 + 3;
              else if (value & 0x10)
                bit = which * 6 + 4;
              else if (value & 0x20)
                bit = which * 6 + 5;
              break;
            }
        }
      which++;
      if (which == (int)len)
        {
          bit = -1;
          break;
        }
      value = sp->u.string[which] - ' ';
    }

  free_string_svalue (sp);
  put_number (bit);
}
#endif


#ifdef F_CLEAR_BIT
void
f_clear_bit (void)
{
  char *str;
  size_t len;
  int ind, bit;

  if (sp->u.number > CONFIG_INT (__MAX_BITFIELD_BITS__))
    error ("clear_bit() bit requested : %d > maximum bits: %d\n",
           sp->u.number, CONFIG_INT (__MAX_BITFIELD_BITS__));
  bit = (int) (sp--)->u.number;
  if (bit < 0)
    error ("Bad argument 2 (negative) to clear_bit().\n");
  ind = bit / 6;
  bit %= 6;
  len = SVALUE_STRLEN (sp);
  if (ind >= (int)len)
    return;			/* return first arg unmodified */
  unlink_string_svalue (sp);
  str = sp->u.string;

  if (str[ind] > 0x3f + ' ' || str[ind] < ' ')
    error ("Illegal bit pattern in clear_bit character %d\n", ind);
  str[ind] = ((str[ind] - ' ') & ~(1 << bit)) + ' ';
}
#endif


#ifdef F_SET_BIT
void
f_set_bit (void)
{
  char *str;
  size_t len, old_len;
  int ind, bit;

  if (sp->u.number > CONFIG_INT (__MAX_BITFIELD_BITS__))
    error ("set_bit() bit requested: %d > maximum bits: %d\n", sp->u.number,
           CONFIG_INT (__MAX_BITFIELD_BITS__));
  bit = (int) (sp--)->u.number;
  if (bit < 0)
    error ("Bad argument 2 (negative) to set_bit().\n");
  ind = bit / 6;
  bit %= 6;
  old_len = len = SVALUE_STRLEN (sp);
  if (ind >= (int)len)
    len = ind + 1;
  if (ind < (int)old_len)
    {
      unlink_string_svalue (sp);
      str = sp->u.string;
    }
  else
    {
      str = new_string (len, "f_set_bit: str");
      str[len] = '\0';
      if (old_len)
        memcpy (str, sp->u.string, old_len);
      if (len > old_len)
        memset (str + old_len, ' ', len - old_len);
      free_string_svalue (sp);
      sp->subtype = STRING_MALLOC;
      sp->u.string = str;
    }

  if (str[ind] > 0x3f + ' ' || str[ind] < ' ')
    error ("Illegal bit pattern in set_bit character %d\n", ind);
  str[ind] = ((str[ind] - ' ') | (1 << bit)) + ' ';
}
#endif

