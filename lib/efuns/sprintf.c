#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

/*
 * sprintf.c v1.05 for LPMud 3.0.52
 *
 * An implementation of (s)printf() for LPC, with quite a few
 * extensions (note that as no floating point exists, some parameters
 * have slightly different meaning or restrictions to "standard"
 * (s)printf.)  Implemented by Lynscar (Sean A Reith).
 * 2/28/93: float support for MudOS added by jacques/blackthorn
 *
 * This version supports the following as modifiers:
 *  " "   pad positive integers with a space.
 *  "+"   pad positive integers with a plus sign.
 *  "-"   left adjusted within field size.
 *        NB: std (s)printf() defaults to right justification, which is
 *            unnatural in the context of a mainly string based language
 *            but has been retained for "compatability" ;)
 *  "|"   centered within field size.
 *  "="   column mode if strings are greater than field size.  this is only
 *        meaningful with strings, all other types ignore
 *        this.  columns are auto-magically word wrapped.
 *  "#"   table mode, print a list of '\n' separated 'words' in a
 *        table within the field size.  only meaningful with strings.
 *   n    specifies the field size, a '*' specifies to use the corresponding
 *        arg as the field size.  if n is prepended with a zero, then is padded
 *        zeros, else it is padded with spaces (or specified pad string).
 *  "."n  presision of n, simple strings truncate after this (if presision is
 *        greater than field size, then field size = presision), tables use
 *        presision to specify the number of columns (if presision not specified
 *        then tables calculate a best fit), all other types ignore this.
 *  ":"n  n specifies the fs _and_ the presision, if n is prepended by a zero
 *        then it is padded with zeros instead of spaces.
 *  "@"   the argument is an array.  the corresponding format_info (minus the
 *        "@") is applyed to each element of the array.
 *  "'X'" The char(s) between the single-quotes are used to pad to field
 *        size (defaults to space) (if both a zero (in front of field
 *        size) and a pad string are specified, the one specified second
 *        overrules).  NOTE:  to include "'" in the pad string, you must
 *        use "\\'" (as the backslash has to be escaped past the
 *        interpreter), similarly, to include "\" requires "\\\\".
 * The following are the possible type specifiers.
 *  "%"   in which case no arguments are interpreted, and a "%" is inserted, and
 *        all modifiers are ignored.
 *  "O"   the argument is an LPC datatype.
 *  "s"   the argument is a string.
 *  "d"   the integer arg is printed in decimal.
 *  "i"   as d.
 *  "f"   floating point value.
 *  "c"   the integer arg is to be printed as a character.
 *  "o"   the integer arg is printed in octal.
 *  "x"   the integer arg is printed in hex.
 *  "X"   the integer arg is printed in hex (in capitals).
 */

#include "src/std.h"
#include "lpc/object.h"
#include "lpc/array.h"
#include "lpc/mapping.h"
#include "lpc/program.h"
#include "src/frame.h"
#include "src/interpret.h"
#include "src/simul_efun.h"

#if defined(F_SPRINTF) || defined(F_PRINTF)

#endif /* defined(F_SPRINTF) || defined(F_PRINTF) */


#ifdef F_SPRINTF
void f_sprintf (void) {
  char *s;
  int num_arg = st_num_arg;

  s = string_print_formatted (SVALUE_STRPTR(sp - num_arg + 1),
                              num_arg - 1, sp - num_arg + 2);
  pop_n_elems (num_arg);

  ++sp;
  if (!s)
    {
      SET_SVALUE_CONSTANT_STRING (sp, "");
    }
  else
    {
      SET_SVALUE_MALLOC_STRING (sp, s);
    }
}
#endif


#ifdef F_PRINTF
void f_printf (void) {
  int num_arg = st_num_arg;
  char *ret;

  if (command_giver)
    {
      ret = string_print_formatted (SVALUE_STRPTR(sp - num_arg + 1),
                                    num_arg - 1, sp - num_arg + 2);
      if (ret)
        {
          tell_object (command_giver, ret);
          FREE_MSTR (ret);
        }
    }

  pop_n_elems (num_arg);
}
#endif
