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
#include "src/interpret.h"
#include "src/simul_efun.h"

#include "lpc/include/function.h"

#include "sprintf.h"

#if defined(F_SPRINTF) || defined(F_PRINTF)

#define	NULL_MSG	"0"

typedef unsigned int format_info;

/*
 * Format of format_info:
 *   00000000 0000xxxx : argument type:
 *				0000 : type not found yet;
 *				0001 : error type not found;
 *				0010 : percent sign, null argument;
 *				0011 : LPC datatype;
 *				0100 : string;
 *				1000 : integer;
 *				1001 : char;
 *				1010 : octal;
 *				1011 : hex;
 *				1100 : HEX;
 *				1101 : float;
 *   00000000 00xx0000 : justification:
 *				00 : right;
 *				01 : centre;
 *				10 : left;
 *   00000000 xx000000 : positive pad char:
 *				00 : none;
 *				01 : ' ';
 *				10 : '+';
 *   0000000x 00000000 : array mode?
 *   000000x0 00000000 : column mode?
 *   00000x00 00000000 : table mode?
 */

#define INFO_T		0xF
#define INFO_T_ERROR	0x1
#define INFO_T_NULL	0x2
#define INFO_T_LPC	0x3
#define INFO_T_STRING	0x4
#define INFO_T_INT	0x8
#define INFO_T_CHAR	0x9
#define INFO_T_OCT	0xA
#define INFO_T_HEX	0xB
#define INFO_T_C_HEX	0xC
#define INFO_T_FLOAT	0xD

#define INFO_J		0x30
#define INFO_J_CENTRE	0x10
#define INFO_J_LEFT	0x20

#define INFO_PP		0xC0
#define INFO_PP_SPACE	0x40
#define INFO_PP_PLUS	0x80

#define INFO_ARRAY	0x100
#define INFO_COLS	0x200
#define INFO_TABLE	0x400

#define ERR_BUFF_OVERFLOW	0x1	/* buffer overflowed */
#define ERR_TO_FEW_ARGS		0x2	/* more arguments spec'ed than passed */
#define ERR_INVALID_STAR	0x3	/* invalid arg to * */
#define ERR_PRES_EXPECTED	0x4	/* expected presision not found */
#define ERR_INVALID_FORMAT_STR	0x5	/* error in format string */
#define ERR_INCORRECT_ARG_S	0x6	/* invalid arg to %s */
#define ERR_CST_REQUIRES_FS	0x7	/* field size not given for c/t */
#define ERR_BAD_INT_TYPE	0x8	/* bad integer type... */
#define ERR_UNDEFINED_TYPE	0x9	/* undefined type found */
#define ERR_QUOTE_EXPECTED	0xA	/* expected ' not found */
#define ERR_UNEXPECTED_EOS	0xB	/* fs terminated unexpectedly */
#define ERR_NULL_PS		0xC	/* pad string is null */
#define ERR_ARRAY_EXPECTED      0xD	/* Yep!  You guessed it. */

#define ADD_CHAR(x) {\
  outbuf_addchar(&obuff, x);\
  if (obuff.real_size == USHRT_MAX) sprintf_error(ERR_BUFF_OVERFLOW); \
}

#define GET_NEXT_ARG {\
  if (++cur_arg >= argc) sprintf_error(ERR_TO_FEW_ARGS); \
  carg = (argv+cur_arg);\
}

typedef struct pad_info_s {
  char *what;
  int len;
} pad_info_t;

typedef struct tab_data_s {
  char *start;
  char *cur;
} tab_data_t;

/* slash here means 'or' */
typedef struct ColumnSlashTable {
  union CSTData
  {
    char *col;			/* column data */
    tab_data_t *tab;		/* table data */
  }
  d;				/* d == data */
  unsigned short int nocols;	/* number of columns in table *sigh* */
  pad_info_t *pad;
  unsigned int start;		/* starting cursor position */
  unsigned int size;		/* column/table width */
  unsigned int remainder;	/* extra space needed to fill out to width */
  int pres;			/* presision */
  format_info info;		/* formatting data */
  struct ColumnSlashTable *next;
} cst;				/* Columns Slash Tables */

static outbuffer_t obuff;
static int guard = 0;
static cst *csts = 0;
static int cur_arg;		/* current arg number */
static svalue_t clean = { .type = T_NUMBER };

static void sprintf_error (int) NO_RETURN;
static void numadd (outbuffer_t *, int64_t num);
static void add_space (outbuffer_t *, int indent);
static void add_justified (char *str, size_t slen, pad_info_t * pad, int fs, format_info finfo, short int trailing);
static int add_column (cst ** column, int trailing);
static int add_table (cst ** table);

/**
 * Signal an LPC sprintf() error.  Note that we call error, so this routine never returns.
 * Anything that has been allocated should be somewhere it can be found and freed later.
 */
static void sprintf_error (int which) NO_RETURN {
  char lbuf[2048];
  char *err;

  switch (which)
    {
    case ERR_BUFF_OVERFLOW:
      err = "BUFF_SIZE overflowed...";
      break;
    case ERR_TO_FEW_ARGS:
      err = "More arguments specified than passed.";
      break;
    case ERR_INVALID_STAR:
      err = "Incorrect argument type to *.";
      break;
    case ERR_PRES_EXPECTED:
      err = "Expected presision not found.";
      break;
    case ERR_INVALID_FORMAT_STR:
      err = "Error in format string.";
      break;
    case ERR_INCORRECT_ARG_S:
      err = "Incorrect argument to type %%s.";
      break;
    case ERR_CST_REQUIRES_FS:
      err = "Column/table mode requires a field size.";
      break;
    case ERR_BAD_INT_TYPE:
      err = "!feature - bad integer type!";
      break;
    case ERR_UNDEFINED_TYPE:
      err = "!feature - undefined type!";
      break;
    case ERR_QUOTE_EXPECTED:
      err = "Quote expected in format string.";
      break;
    case ERR_UNEXPECTED_EOS:
      err = "Unexpected end of format string.";
      break;
    case ERR_NULL_PS:
      err = "Null pad string specified.";
      break;
    case ERR_ARRAY_EXPECTED:
      err = "Array expected.";
      break;
    default:
      err = "undefined error in (s)printf!\n";
      break;
    }
  sprintf (lbuf, "(s)printf(): %s (arg: %d)\n", err, cur_arg);
  error (lbuf);
}

/**
 * Output number num to outbuf as decimal.
 */
static void numadd (outbuffer_t * outbuf, int64_t num) {
  int64_t i;
  size_t num_l; /* length of num as a string */
  int nve;      /* true if num negative */
  size_t space;
  size_t chop;
  char *p;

  if (num < 0)
    {
      num = llabs (num);
      nve = 1;
    }
  else
    nve = 0;
  for (i = num / 10, num_l = nve + 1; i; i /= 10, num_l++);
  if ((space = outbuf_extend (outbuf, num_l)))
    {
      chop = num_l - space;
      while (chop--)
        num /= 10;		/* lose that last digits that got chopped */
      p = outbuf->buffer + outbuf->real_size;
      outbuf->real_size += space;
      p[space] = 0;
      if (nve)
        {
          *p++ = '-';
          space--;
        }
      while (space--)
        {
          p[space] = (num % 10) + '0';
          num /= 10;
        }
    }
}

static void add_space (outbuffer_t * outbuf, int indent) {
  size_t l;

  if ((l = outbuf_extend (outbuf, indent)))
    {
      memset (outbuf->buffer + outbuf->real_size, ' ', l);
      *(outbuf->buffer + outbuf->real_size + l) = 0;
      outbuf->real_size += l;
    }
}

/**
 * Converts any LPC datatype into an arbitrary string format
 * and returns a pointer to this string.
 * Scary number of parameters for a recursive function.
 */
void svalue_to_string (svalue_t * obj, outbuffer_t * outbuf, int indent, char delimiter, int flags) {
  int i;

  /* prevent an infinite recursion on self-referential structures */
  if (indent > 20)
    {
      outbuf_add (outbuf, "...");
      return;
    }

  if (indent > 0 &&
      0==(flags & SV2STR_NOINDENT) &&
      0==(flags & SV2STR_DONEINDENT))
    add_space (outbuf, indent);

  flags &= ~SV2STR_DONEINDENT;	/* do not pass this down recursion */

  switch (obj->type)
    {
    case T_INVALID:
      outbuf_add (outbuf, "T_INVALID");
      break;

    case T_LVALUE:
      outbuf_add (outbuf, "lvalue: ");
      svalue_to_string (obj->u.lvalue, outbuf, indent + 2, delimiter, flags);
      break;

    case T_NUMBER:
      numadd (outbuf, obj->u.number);
      break;

    case T_REAL:
      outbuf_addv (outbuf, "%g", obj->u.real);
      break;

    case T_STRING:
      {
        const char* str;
        outbuf_add (outbuf, "\"");
        for (str = obj->u.string; *str; ++str)
          {
            switch (*str)
              {
                case '\t':
                  outbuf_add (outbuf, "\\t");
                  break;
                case '\n':
                  outbuf_add (outbuf, "\\n");
                  break;
                case '\r':
                  outbuf_add (outbuf, "\\r");
                  break;
                case '\b':
                  outbuf_add (outbuf, "\\b");
                  break;
                case '\x7':
                  outbuf_add (outbuf, "\\a");
                  break;
                case '\x1b':
                  outbuf_add (outbuf, "\\e");
                  break;
                case '\\':
                  outbuf_add (outbuf, "\\\\");
                  break;
                case '"':
                  outbuf_add (outbuf, "\\\"");
                  break;
                default:
                  if (iscntrl (*str))
                    outbuf_addv (outbuf, "\\0%o", *str);
                  else
                    outbuf_addchar (outbuf, *str);
                  break;
              }
          }
        outbuf_add (outbuf, "\"");
        break;
      }

    case T_CLASS:
      {
        int n = obj->u.arr->size;
        outbuf_add (outbuf, "CLASS( ");
        numadd (outbuf, n);
        outbuf_add (outbuf, n == 1 ? " element\n" : " elements\n");
        for (i = 0; i < (obj->u.arr->size) - 1; i++)
          svalue_to_string (&(obj->u.arr->item[i]), outbuf, indent + 2, ',', flags);
        svalue_to_string (&(obj->u.arr->item[i]), outbuf, indent + 2, 0, flags);
        if (0==(flags & SV2STR_NONEWLINE))
          outbuf_addchar (outbuf, '\n');

        if (indent > 0 && 0==(flags & SV2STR_NOINDENT))
          add_space (outbuf, indent);
        outbuf_add (outbuf, " )");
        break;
      }

    case T_ARRAY:
      if (!(obj->u.arr->size))
        {
          outbuf_add (outbuf, "({ })");
        }
      else
        {
          outbuf_add (outbuf, "({");
          if (0==(flags & SV2STR_NONEWLINE))
            outbuf_addchar (outbuf, '\n');
#if 0
          outbuf_add (outbuf, "({ /* sizeof() == ");
          numadd (outbuf, obj->u.arr->size);
          outbuf_add (outbuf, " */\n");
#endif
          for (i = 0; i < obj->u.arr->size; i++)
            {
              svalue_to_string (
                &(obj->u.arr->item[i]), outbuf, indent + 2,
                (i==obj->u.arr->size-1 && (flags & SV2STR_NOINDENT))?0:',',
                flags
              );
            }
          if (indent > 0 && 0==(flags & SV2STR_NOINDENT))
            add_space (outbuf, indent);
          outbuf_add (outbuf, "})");
        }
      break;

    case T_BUFFER:
      outbuf_add (outbuf, "<buffer>");
      break;

    case T_FUNCTION:
      {
        outbuf_add (outbuf, "(: ");
        switch (obj->u.fp->hdr.type)
          {
          case FP_LOCAL | FP_NOT_BINDABLE:
            outbuf_add (outbuf, function_name (obj->u.fp->hdr.owner->prog, obj->u.fp->f.local.index));
            break;
          case FP_SIMUL:
            outbuf_add (outbuf, simuls[obj->u.fp->f.simul.index].func->name);
            break;
          case FP_FUNCTIONAL:
          case FP_FUNCTIONAL | FP_NOT_BINDABLE:
            {
              char buf[10];
              int n = obj->u.fp->f.functional.num_arg;

              outbuf_add (outbuf, "<code>(");
              for (i = 1; i < n; i++)
                {
                  sprintf (buf, "$%i, ", i);
                  outbuf_add (outbuf, buf);
                }
              if (n)
                {
                  sprintf (buf, "$%i", n);
                  outbuf_add (outbuf, buf);
                }
              outbuf_add (outbuf, ")");
              break;
            }
          case FP_EFUN:
            {
              i = obj->u.fp->f.efun.index;
              outbuf_add (outbuf, instrs[i].name);
              break;
            }
          }
        if (obj->u.fp->hdr.args)
          {
            for (i = 0; i < obj->u.fp->hdr.args->size; i++)
              {
                outbuf_add (outbuf, ", ");
                svalue_to_string (&(obj->u.fp->hdr.args->item[i]), outbuf, indent, 0, flags);
              }
          }
      }
      outbuf_add (outbuf, " :)");
      break;

    case T_MAPPING:
      if (!(obj->u.map->count))
        {
          outbuf_add (outbuf, "([ ])");
        }
      else
        {
          int count = 0;

          outbuf_add (outbuf, "([");
          if (0==(flags & SV2STR_NONEWLINE))
            outbuf_addchar (outbuf, '\n');
#if 0
          outbuf_add (outbuf, "([ /* ");
          numadd (outbuf, obj->u.map->count);
          outbuf_add (outbuf, " element(s) */\n");
#endif
          for (i = 0; i <= (int) (obj->u.map->table_size); i++)
            {
              mapping_node_t *elm;

              for (elm = obj->u.map->table[i]; elm; elm = elm->next)
                {
                  svalue_to_string (&(elm->values[0]), outbuf, indent + 2, ':', flags | SV2STR_NONEWLINE);
                  svalue_to_string (
                    &(elm->values[1]), outbuf, indent + 2,
                    ((++count==obj->u.map->count) && (flags & SV2STR_NOINDENT)) ? 0 : ',',
                    flags | SV2STR_DONEINDENT
                  );
                }
            }
          if (indent > 0 && 0==(flags & SV2STR_NOINDENT))
            add_space (outbuf, indent);
          outbuf_add (outbuf, "])");
        }
      break;

    case T_OBJECT:
      {
        svalue_t *temp;

        if (obj->u.ob->flags & O_DESTRUCTED)
          {
            numadd (outbuf, 0);
            break;
          }

        outbuf_add (outbuf, obj->u.ob->name);

        if (!get_error_state (ES_STACK_FULL))
          {
            push_object (obj->u.ob);
            guard = 1;
            temp = safe_apply_master_ob (APPLY_OBJECT_NAME, 1);
            guard = 0;
            if (temp && temp != (svalue_t *) - 1 && (temp->type == T_STRING))
              {
                outbuf_add (outbuf, " (\"");
                outbuf_add (outbuf, temp->u.string);
                outbuf_add (outbuf, "\")");
              }
          }
        break;
      }
    default:
      outbuf_add (outbuf, "!ERROR: garbage svalue!");
    }				/* end of switch (obj->type) */

  if (delimiter)
    {
      outbuf_addchar (outbuf, delimiter);
      if (0==(flags & SV2STR_NONEWLINE))
        outbuf_addchar (outbuf, '\n');
      else
        outbuf_addchar (outbuf, ' ');
        
    }
}				/* end of svalue_to_string() */

static void add_pad (pad_info_t * pad, size_t len) {
  char *p;
  int padlen;

  if (outbuf_extend (&obuff, len) != len)
    sprintf_error (ERR_BUFF_OVERFLOW);
  p = obuff.buffer + obuff.real_size;
  obuff.real_size += len;
  p[len] = 0;

  if (pad && (padlen = pad->len))
    {
      char *end;
      char *pstr = pad->what;
      int i;
      char c;

      for (i = 0, end = p + len; p < end; i++)
        {
          if (i == padlen)
            i = 0;

          if ((c = pstr[i]) == '\\')
            {
              /* guaranteed to have a valid char next */
              *p++ = pstr[++i];
            }
          else
            *p++ = c;
        }
    }
  else
    memset (p, ' ', len);
}

static void add_nstr (char *str, size_t len) {
  if (outbuf_extend (&obuff, len) != len)
    sprintf_error (ERR_BUFF_OVERFLOW);
  memcpy (obuff.buffer + obuff.real_size, str, len);
  obuff.real_size += len;
  obuff.buffer[obuff.real_size] = 0;
}

/**
 * Adds the string "str" to the buff after justifying it within field size "fs".
 * Field size "fs" is adjusted to take into account any ANSI escape sequences
 * within "str".
 * "trailing" is a flag which is set if trailing justification is to be done.
 * "str" is added verbatim.
 * trailing is, of course, ignored in the case of right justification.
 * 
 * @param str The string to be justified.
 * @param slen The length of the string.
 * @param pad The pad info to use.
 * @param fs The field size.
 * @param finfo The format info.
 * @param trailing A flag indicating if trailing justification is to be done.
 */
static void add_justified (char *str, size_t slen, pad_info_t * pad, int fs, format_info finfo, short int trailing) {
  /* compensate field size by adding count of characters for ANSI escape sequences */
  const char *pstr;
  for (pstr = str; pstr - str < (ptrdiff_t)slen;)
    {
      if (*pstr == '\x1B') /* ESC */
        {
          pstr++;
          fs += 2;
          if (*pstr++ == '[')
            {
              while (isdigit (*pstr) || *pstr == ';')
                {
                  pstr++;
                  fs++;
                }
              pstr++;
              fs++;
            }
          continue;
        }
      pstr++;
    }

  fs -= (int)slen;
  if (fs <= 0)
    {
      add_nstr (str, slen);
    }
  else
    {
      /* add paddings */
      int i;
      switch (finfo & INFO_J)
        {
        case INFO_J_LEFT:
          add_nstr (str, slen);
          if (trailing)
            add_pad (pad, fs);
          break;
        case INFO_J_CENTRE:
          i = fs / 2 + fs % 2;
          add_pad (pad, i);
          add_nstr (str, slen);
          if (trailing)
            add_pad (pad, fs - i);
          break;
        default:
          /* std (s)printf defaults to right
           * justification */
          add_pad (pad, fs);
          add_nstr (str, slen);
        }
    }
}

/*
 * Adds "column" to the buffer.
 * Returns 0 is column not finished.
 * Returns 1 if column completed.
 * Returns 2 if column completed has a \n at the end.
 */
static int add_column (cst ** column, int trailing) {
  register unsigned int done;
  char c;
  unsigned int space = (unsigned int)-1;
  int ret;
  cst *col = *column;		/* always holds (*column) */
  char *col_d = col->d.col;	/* always holds (col->d.col) */

  done = 0;
  /* find a good spot to break the line */
  while ((c = col_d[done]) && c != '\n')
    {
      if (c == ' ')
        space = done;
      if (++done == (unsigned int)col->pres)
        {
          if (space != (unsigned int)-1)
            {
              c = col_d[done];
              if (c != '\n' && c != ' ' && c)
                done = space;
            }
          break;
        }
    }
  add_justified (col_d, done, col->pad,
                 col->size, col->info, trailing || col->next);
  col_d += done;
  ret = 1;
  if (*col_d == '\n')
    {
      col_d++;
      ret = 2;
    }
  col->d.col = col_d;
  /*
   * if the next character is a NULL then take this column out of
   * the list.
   */
  if (!(*col_d))
    {
      cst *temp;

      temp = col->next;
      if (col->pad)
        FREE (col->pad);
      FREE (col);
      *column = temp;
      return ret;
    }
  return 0;
}				/* end of add_column() */

/*
 * Adds "table" to the buffer.
 * Returns 0 if table not completed.
 * Returns 1 if table completed.
 */
static int add_table (cst ** table) {
  int done, i;
  cst *tab = *table; /* always (*table) */
  tab_data_t *tab_d = tab->d.tab; /* always tab->d.tab */
  char *tab_di; /* always tab->d.tab[i].cur */
  ptrdiff_t end;

  for (i = 0; i < tab->nocols && (tab_di = tab_d[i].cur); i++)
    {
      end = tab_d[i + 1].start - tab_di - 1;

      for (done = 0; done != end && tab_di[done] != '\n'; done++)
        ;
      add_justified (tab_di, (done > (int)tab->size ? (int)tab->size : done),
                     tab->pad, tab->size, tab->info,
                     tab->pad || (i < tab->nocols - 1) || tab->next);
      if (done >= end - 1)
        {
          tab_di = 0;
        }
      else
        {
          tab_di += done + 1;	/* inc'ed next line ... */
        }
      tab_d[i].cur = tab_di;
    }
  if (tab->pad)
    {
      while (i++ < tab->nocols)
        {
          add_pad (tab->pad, tab->size);
        }
      add_pad (tab->pad, tab->remainder);
    }
  if (!tab_d[0].cur)
    {
      cst *temp;

      temp = tab->next;
      if (tab->pad)
        FREE (tab->pad);
      if (tab_d)
        FREE (tab_d);
      FREE (tab);
      *table = temp;
      return 1;
    }
  return 0;
}				/* end of add_table() */

static int get_curpos () {
  char *p1, *p2;

  if (!obuff.buffer)
    return 0;
  p1 = obuff.buffer + obuff.real_size - 1;
  p2 = p1;
  while (p2 > obuff.buffer && *p2 != '\n')
    p2--;
  if (*p2 != '\n')
    return (int)(p1 - p2 + 1);
  else
    return (int)(p1 - p2);
}

/* We can't use a pointer to a local in a table or column, since it
 * could get overwritten by another on the same line.
 */
pad_info_t* make_pad (pad_info_t * p) {
  pad_info_t *x;
  if (p->len == 0)
    return 0;
  x = ALLOCATE (pad_info_t, TAG_TEMPORARY, "make_pad");
  x->what = p->what;
  x->len = p->len;
  return x;
}

/*
 * THE (s)printf() function.
 * It returns a pointer to it's internal buffer (or a string in the text
 * segment) thus, the string must be copied if it has to survive after
 * this function is called again, or if it's going to be modified (esp.
 * if it risks being free()ed).
 */
char* string_print_formatted (char *format_str, int argc, svalue_t * argv) {
  format_info finfo;
  svalue_t *carg;		/* current arg */
  unsigned int nelemno = 0;	/* next offset into array */
  unsigned int fpos;		/* position in format_str */
  int fs;			/* field size */
  int pres;			/* presision */
  pad_info_t pad;		/* fs pad string */
  unsigned int i;
  char *retvalue;
  int last;

  /* prevent recursion, since many of our important variables are global */
  if (guard)
    {
      guard = 0;
      error ("Illegal to use sprintf() within the object_name() master call.\n");
    }

  /* free anything that is sitting around here from errors */
  if (clean.type != T_NUMBER)
    {
      free_svalue (&clean, "sprintf error");
      clean.type = T_NUMBER;
    }

  /* The string we construct */
  if (obuff.buffer)
    FREE_MSTR (obuff.buffer);
  outbuf_zero (&obuff);

  /* Table info */
  while (csts)
    {
      cst *next = csts->next;
      if (!(csts->info & INFO_COLS) && csts->d.tab)
        FREE (csts->d.tab);
      FREE (csts);
      csts = next;
    }

  cur_arg = -1;
  last = 0;
  for (fpos = 0; 1; fpos++)
    {
      char c = format_str[fpos];

      if (c == '\n' || !c)
        {
          int column_stat = 0;

          if (last != (int)fpos)
            {
              add_nstr (format_str + last, fpos - last);
              last = fpos + 1;
            }
          else
            last++;

          if (!csts)
            {
              if (!c)
                break;
              ADD_CHAR ('\n');
              continue;
            }
          ADD_CHAR ('\n');
          while (csts)
            {
              cst **temp;

              temp = &csts;
              while (*temp)
                {
                  if ((*temp)->info & INFO_COLS)
                    {
                      if (*((*temp)->d.col - 1) != '\n')
                        while (*((*temp)->d.col) == ' ')
                          (*temp)->d.col++;
                      add_pad (0, (*temp)->start - get_curpos ());
                      column_stat = add_column (temp, 0);
                      if (!column_stat)
                        temp = &((*temp)->next);
                    }
                  else
                    {
                      add_pad (0, (*temp)->start - get_curpos ());
                      if (!add_table (temp))
                        temp = &((*temp)->next);
                    }
                }		/* of while (*temp) */
              if (csts || c == '\n')
                ADD_CHAR ('\n');
            }			/* of while (csts) */
          if (column_stat == 2)
            ADD_CHAR ('\n');
          if (!c)
            break;
        }
      else if (c == '%')
        {
          if (last != (int)fpos)
            {
              add_nstr (format_str + last, fpos - last);
              last = fpos + 1;
            }
          else
            last++;

          if (format_str[fpos + 1] == '%')
            {
              ADD_CHAR ('%');
              fpos++;
              last++;
              continue;
            }

          GET_NEXT_ARG;
          fs = 0;
          pres = 0;
          pad.len = 0;
          finfo = 0;
          for (fpos++; !(finfo & INFO_T); fpos++)
            {
              if (!format_str[fpos])
                {
                  finfo |= INFO_T_ERROR;
                  break;
                }
              if (((format_str[fpos] >= '0') && (format_str[fpos] <= '9'))
                  || (format_str[fpos] == '*'))
                {
                  if (pres == -1)
                    {
                      if (format_str[fpos] == '*')
                        {
                          if (carg->type != T_NUMBER)
                            sprintf_error (ERR_INVALID_STAR);
                          pres = (int)carg->u.number;
                          GET_NEXT_ARG;
                          continue;
                        }
                      pres = format_str[fpos] - '0';
                      for (fpos++; isdigit (format_str[fpos]); fpos++)
                        pres = pres * 10 + format_str[fpos] - '0';
                      if (pres < 0)
                        pres = 0;
                    }
                  else
                    {
                      if ((format_str[fpos] == '0')
                          &&
                          (((format_str
                             [fpos + 1] >= '1')
                            && (format_str[fpos + 1] <= '9'))
                           || (format_str[fpos + 1] == '*')))
                        {
                          pad.what = "0";
                          pad.len = 1;
                        }
                      else
                        {
                          if (format_str[fpos] == '*')
                            {
                              if (carg->type != T_NUMBER)
                                sprintf_error (ERR_INVALID_STAR);
                              fs = (int)carg->u.number;
                              if (fs < 0)
                                fs = 0;
                              if (pres == -2)
                                pres = fs;	/* colon */
                              GET_NEXT_ARG;
                              continue;
                            }
                          fs = format_str[fpos] - '0';
                        }
                      for (fpos++; isdigit (format_str[fpos]); fpos++)
                        fs = fs * 10 + format_str[fpos] - '0';
                      if (fs < 0)
                        fs = 0;
                      if (pres == -2)
                        {	/* colon */
                          pres = fs;
                        }
                    }
                  fpos--;	/* about to get incremented */
                  continue;
                }
              switch (format_str[fpos])
                {
                case ' ':
                  finfo |= INFO_PP_SPACE;
                  break;
                case '+':
                  finfo |= INFO_PP_PLUS;
                  break;
                case '-':
                  finfo |= INFO_J_LEFT;
                  break;
                case '|':
                  finfo |= INFO_J_CENTRE;
                  break;
                case '@':
                  finfo |= INFO_ARRAY;
                  break;
                case '=':
                  finfo |= INFO_COLS;
                  break;
                case '#':
                  finfo |= INFO_TABLE;
                  break;
                case '.':
                  pres = -1;
                  break;
                case ':':
                  pres = -2;
                  break;
                case 'O':
                  finfo |= INFO_T_LPC;
                  break;
                case 's':
                  finfo |= INFO_T_STRING;
                  break;
                case 'd':
                case 'i':
                  finfo |= INFO_T_INT;
                  break;
                case 'f':
                  finfo |= INFO_T_FLOAT;
                  break;
                case 'c':
                  finfo |= INFO_T_CHAR;
                  break;
                case 'o':
                  finfo |= INFO_T_OCT;
                  break;
                case 'x':
                  finfo |= INFO_T_HEX;
                  break;
                case 'X':
                  finfo |= INFO_T_C_HEX;
                  break;
                case '\'':
                  fpos++;
                  pad.what = format_str + fpos;
                  while (1)
                    {
                      if (!format_str[fpos])
                        sprintf_error (ERR_UNEXPECTED_EOS);
                      if (format_str[fpos] == '\\')
                        {
                          if (!format_str[++fpos])
                            sprintf_error (ERR_UNEXPECTED_EOS);
                        }
                      else if (format_str[fpos] == '\'')
                        {
                          pad.len = (int)(format_str + fpos - pad.what);
                          if (!pad.len)
                            sprintf_error (ERR_NULL_PS);
                          break;
                        }
                      fpos++;
                    }
                  break;
                default:
                  finfo |= INFO_T_ERROR;
                }
            }			/* end of for () */
          if (pres < 0)
            sprintf_error (ERR_PRES_EXPECTED);
          /*
           * now handle the different arg types...
           */
          if (finfo & INFO_ARRAY)
            {
              if (carg->type != T_ARRAY)
                sprintf_error (ERR_ARRAY_EXPECTED);
              if (carg->u.arr->size == 0)
                {
                  last = fpos;
                  fpos--;	/* 'bout to get incremented */
                  continue;
                }
              carg = (argv + cur_arg)->u.arr->item;
              nelemno = 1;	/* next element number */
            }
          while (1)
            {
              if ((finfo & INFO_T) == INFO_T_LPC)
                {
                  outbuffer_t outbuf;

                  outbuf_zero (&outbuf);
                  svalue_to_string (carg, &outbuf, 0, 0, 0);
                  outbuf_fix (&outbuf);

                  clean.type = T_STRING;
                  clean.subtype = STRING_MALLOC;
                  clean.u.string = outbuf.buffer;
                  carg = &clean;
                  finfo ^= INFO_T_LPC;
                  finfo |= INFO_T_STRING;
                }
              if ((finfo & INFO_T) == INFO_T_ERROR)
                {
                  sprintf_error (ERR_INVALID_FORMAT_STR);
                }
              else if ((finfo & INFO_T) == INFO_T_STRING)
                {
                  size_t slen;

                  /*
                   * %s null handling added 930709 by Luke Mewburn
                   * <zak@rmit.oz.au>
                   */
                  if (carg->type == T_NUMBER && carg->u.number == 0)
                    {
                      clean.type = T_STRING;
                      clean.subtype = STRING_MALLOC;
                      clean.u.string = string_copy (NULL_MSG, "sprintf NULL");
                      carg = &clean;
                    }
                  else if (carg->type != T_STRING)
                    {
                      sprintf_error (ERR_INCORRECT_ARG_S);
                    }
                  slen = SVALUE_STRLEN (carg);
                  if ((finfo & INFO_COLS) || (finfo & INFO_TABLE))
                    {
                      cst **temp;

                      if (!fs)
                        sprintf_error (ERR_CST_REQUIRES_FS);

                      temp = &csts;
                      while (*temp)
                        temp = &((*temp)->next);
                      if (finfo & INFO_COLS)
                        {
                          int tmp;
                          if (pres > fs)
                            pres = fs;
                          *temp =
                            ALLOCATE (cst, TAG_TEMPORARY, "string_print: 3");
                          (*temp)->next = 0;
                          (*temp)->d.col = carg->u.string;
                          (*temp)->pad = make_pad (&pad);
                          (*temp)->size = fs;
                          (*temp)->pres = (pres) ? pres : fs;
                          (*temp)->info = finfo;
                          (*temp)->start = get_curpos ();
                          tmp = ((format_str[fpos] != '\n')
                                 && (format_str[fpos] != '\0'))
                            || ((finfo & INFO_ARRAY)
                                && (nelemno < (argv + cur_arg)->u.arr->size));
                          tmp = add_column (temp, tmp);
                          if (tmp == 2 && !format_str[fpos])
                            {
                              ADD_CHAR ('\n');
                            }
                        }
                      else
                        {	/* (finfo & INFO_TABLE) */
                          unsigned int n, len, max_len;
                          char *p1, *p2;

#define TABLE carg->u.string

                          (*temp) = ALLOCATE (cst, TAG_TEMPORARY, "string_print: 4");
                          (*temp)->d.tab = 0;
                          (*temp)->pad = make_pad (&pad);
                          (*temp)->info = finfo;
                          (*temp)->start = get_curpos ();
                          (*temp)->next = 0;
                          max_len = 0;
                          n = 1;

                          p2 = p1 = TABLE;
                          while (*p1)
                            {
                              if (*p1 == '\n')
                                {
                                  if (p1 - p2 > max_len)
                                    max_len = (unsigned int)(p1 - p2);
                                  p1++;
                                  if (*(p2 = p1))
                                    n++;
                                }
                              else
                                p1++;
                            }
                          if (!pres)
                            {
                              /* the null terminated word */
                              if (p1 - p2 > max_len)
                                max_len = (unsigned int)(p1 - p2);
                              pres = fs / (max_len + 2);	/* at least two
                                                                 * separating spaces */
                              if (!pres)
                                pres = 1;

                              /* This moves some entries from the right side
                               * of the table to fill out the last line,
                               * which makes the table look a bit nicer.
                               * E.g.
                               * (n=13,p=6)      (l=3,p=5)
                               * X X X X X X     X X X X X
                               * X X X X X X  -> X X X X X
                               * X               X X X X
                               *
                               */
                              len = (n - 1) / pres + 1;
                              if (n > (unsigned int)pres && n % pres)
                                pres -= (pres - n % pres) / len;
                            }
                          else
                            {
                              len = (n - 1) / pres + 1;
                            }
                          (*temp)->size = fs / pres;
                          (*temp)->remainder = fs % pres;
                          if (n < (unsigned int)pres)
                            {
                              /* If we have fewer elements than columns,
                               * pretend we are dealing with a smaller
                               * table.
                               */
                              (*temp)->remainder +=
                                (pres - n) * ((*temp)->size);
                              pres = n;
                            }

                          (*temp)->d.tab = CALLOCATE (pres + 1, tab_data_t, TAG_TEMPORARY, "string_print: 5");
                          (*temp)->nocols = (unsigned short)pres;	/* heavy sigh */
                          (*temp)->d.tab[0].start = TABLE;
                          if (pres == 1)
                            {

                              (*temp)->d.tab[1].start =
                                TABLE + SVALUE_STRLEN (carg) + 1;
                            }
                          else
                            {
                              i = 1;	/* the next column number */
                              n = 0;	/* the current "word" number in this
                                         * column */

                              p1 = TABLE;
                              while (*p1)
                                {
                                  if (*p1++ == '\n' && ++n >= len)
                                    {
                                      (*temp)->d.tab[i++].start = p1;
                                      n = 0;
                                    }
                                }
                              for (; i <= (unsigned int)pres; i++)
                                (*temp)->d.tab[i].start = ++p1;
                            }
                          for (i = 0; i < (unsigned int)pres; i++)
                            (*temp)->d.tab[i].cur = (*temp)->d.tab[i].start;

                          add_table (temp);
                        }
                    }
                  else
                    {		/* not column or table */
                      if (pres && pres < slen)
                        slen = pres;
                      add_justified (carg->u.string, slen, &pad, fs, finfo, (
                        ((format_str[fpos] != '\n') && (format_str[fpos] != '\0')) ||
                        ((finfo & INFO_ARRAY) && (nelemno < (argv + cur_arg)->u.arr->size))
                        ) ||
                        carg->u.string[slen - 1] != '\n'
                      );
                    }
                }
              else if (finfo & INFO_T_INT)
                {		/* one of the integer
                                 * types */
                  char cheat[8];
                  char temp[100];

                  *cheat = '%';
                  i = 1;
                  switch (finfo & INFO_PP)
                    {
                    case INFO_PP_SPACE:
                      cheat[i++] = ' ';
                      break;
                    case INFO_PP_PLUS:
                      cheat[i++] = '+';
                      break;
                    }
                  if (pres)
                    {
                      cheat[i++] = '.';
                      sprintf (cheat + i, "%d", pres);
                      i += (int)strlen (cheat + i);
                    }
                  switch (finfo & INFO_T)
                    {
                    case INFO_T_INT:
                      cheat[i++] = 'd';
                      break;
                    case INFO_T_FLOAT:
                      cheat[i++] = 'f';
                      break;
                    case INFO_T_CHAR:
                      cheat[i++] = 'c';
                      break;
                    case INFO_T_OCT:
                      cheat[i++] = 'o';
                      break;
                    case INFO_T_HEX:
                      cheat[i++] = 'x';
                      break;
                    case INFO_T_C_HEX:
                      cheat[i++] = 'X';
                      break;
                    default:
                      sprintf_error (ERR_BAD_INT_TYPE);
                    }
                  if ((cheat[i - 1] == 'f' && carg->type != T_REAL)
                      || (cheat[i - 1] != 'f' && carg->type != T_NUMBER))
                    {
                      error
                        ("ERROR: (s)printf(): Incorrect argument type to %%%c.\n",
                         cheat[i - 1]);
                    }
                  cheat[i] = '\0';

                  if (carg->type == T_REAL)
                    {
                      sprintf (temp, cheat, carg->u.real);
                    }
                  else
                    sprintf (temp, cheat, carg->u.number);
                  {
                    int tmpl = (int)strlen (temp);

                    add_justified (temp, tmpl, &pad, fs, finfo,
                                   (((format_str[fpos] != '\n') && (format_str[fpos] != '\0'))
                                    || ((finfo & INFO_ARRAY) && (nelemno < (argv + cur_arg)->u.arr->size))));
                  }
                }
              else		/* type not found */
                sprintf_error (ERR_UNDEFINED_TYPE);
              if (clean.type != T_NUMBER)
                {
                  free_svalue (&clean, "string_print_formatted");
                  clean.type = T_NUMBER;
                }

              if (!(finfo & INFO_ARRAY))
                break;
              if (nelemno >= (argv + cur_arg)->u.arr->size)
                break;
              carg = (argv + cur_arg)->u.arr->item + nelemno++;
            }			/* end of while (1) */
          last = fpos;
          fpos--;		/* bout to get incremented */
        }
    }				/* end of for (fpos=0; 1; fpos++) */

  outbuf_fix (&obuff);
  retvalue = obuff.buffer;
  obuff.buffer = 0;
  return retvalue;
}				/* end of string_print_formatted() */

#endif /* defined(F_SPRINTF) || defined(F_PRINTF) */


#ifdef F_SPRINTF
void f_sprintf (void) {
  char *s;
  int num_arg = st_num_arg;

  s = string_print_formatted ((sp - num_arg + 1)->u.string,
                              num_arg - 1, sp - num_arg + 2);
  pop_n_elems (num_arg);

  (++sp)->type = T_STRING;
  if (!s)
    {
      sp->subtype = STRING_CONSTANT;
      sp->u.string = "";
    }
  else
    {
      sp->subtype = STRING_MALLOC;
      sp->u.string = s;
    }
}
#endif


#ifdef F_PRINTF
void f_printf (void) {
  int num_arg = st_num_arg;
  char *ret;

  if (command_giver)
    {
      ret = string_print_formatted ((sp - num_arg + 1)->u.string,
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
