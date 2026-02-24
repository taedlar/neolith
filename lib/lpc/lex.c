#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

/* Revision:
 * 93-06-27 (Robocoder):
 *   Adjusted the meaning of the EXPECT_* flags;
 *     EXPECT_ELSE  ... means the last condition was false, so we want to find
 *                      an alternative or the end of the conditional block
 *     EXPECT_ENDIF ... means the last condition was true, so we want to find
 *                      the end of the conditional block
 *   Added #elif preprocessor command
 *   Fixed get_text_block bug so no text returned ""
 *   Added get_array_block()...using @@ENDMARKER to return array of strings
 */

#define SUPPRESS_COMPILER_INLINES
#include "src/std.h"
#include "rc.h"
#include "hash.h"
#include "lex.h"
#include "compiler.h"
#include "scratchpad.h"
#include "lpc/include/function.h"
#include "efuns/file_utils.h"

#include "preprocess.h"
#include "lib/lpc/grammar.h"

#define SKIPWHITE while (isspace(*p) && (*p != '\n')) p++

#define NELEM(a) (sizeof (a) / sizeof((a)[0]))
#define LEX_EOF ((char) EOF)

char lex_ctype[256] =
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0
};

#define is_wspace(c) lex_ctype[(unsigned char)(c)]

int current_line;		/* line number in this file */
int current_line_base;		/* number of lines from other files */
int current_line_saved;		/* last line in this file where line num
                                   info was saved */
int total_lines;		/* Used to compute average compiled lines/s */
char *current_file;
int current_file_id;

/* Bit flags for pragmas in effect */
int pragmas;

int num_parse_error;		/* Number of errors in the parser. */

lpc_predef_t *lpc_predefs = NULL;

static int yyin_desc;
static int lex_fatal;
static char **inc_list = NULL;
static int inc_list_size = 0;
static int defines_need_freed = 0;
static char *last_nl;
static int nexpands = 0;

#define EXPANDMAX 25000

char yytext[MAXLINE];
static char *outptr;

typedef struct incstate_s
{
  struct incstate_s *next;
  int yyin_desc;
  int line;
  char *file;
  int file_id;
  char *last_nl;
  char *outptr;
}
incstate_t;

static incstate_t *inctop = 0;

/* prevent unbridled recursion */
#define MAX_INCLUDE_DEPTH 32
static int incnum;

/* If more than this is needed, the code needs help :-) */
#define MAX_FUNCTION_DEPTH 10

static function_context_t function_context_stack[MAX_FUNCTION_DEPTH];
static int last_function_context;
function_context_t *current_function_context = 0;

/*
 * The number of arguments stated below, are used by the compiler.
 * If min == max, then no information has to be coded about the
 * actual number of arguments. Otherwise, the actual number of arguments
 * will be stored in the byte after the instruction.
 * A maximum value of -1 means unlimited maximum value.
 *
 * If an argument has type 0 (T_INVALID) specified, then no checks will
 * be done at run time.
 *
 * The argument types are currently not checked by the compiler,
 * only by the runtime.
 */

/* keyword_t predefs[] = */
#include "efuns_definition.h"
/* char *option_defs[] = */
#include "efuns_option.h"

static keyword_t reswords[] = {
  {.word = "asm", 0, 0},
  {.word = "break", L_BREAK, 0},
  {.word = "buffer", L_BASIC_TYPE, TYPE_BUFFER},
  {.word = "case", L_CASE, 0},
  {.word = "catch", L_CATCH, 0},
  {.word = "class", L_CLASS, 0},
  {.word = "continue", L_CONTINUE, 0},
  {.word = "default", L_DEFAULT, 0},
  {.word = "do", L_DO, 0},
  {.word = "efun", L_EFUN, 0},
  {.word = "else", L_ELSE, 0},
  {.word = "float", L_BASIC_TYPE, TYPE_REAL},
  {.word = "double", L_BASIC_TYPE, TYPE_REAL},
  {.word = "for", L_FOR, 0},
  {.word = "foreach", L_FOREACH, 0},
  {.word = "function", L_BASIC_TYPE, TYPE_FUNCTION},
  {.word = "if", L_IF, 0},
  {.word = "in", L_IN, 0},
  {.word = "inherit", L_INHERIT, 0},
  {.word = "int", L_BASIC_TYPE, TYPE_NUMBER},
  {.word = "mapping", L_BASIC_TYPE, TYPE_MAPPING},
  {.word = "mixed", L_BASIC_TYPE, TYPE_ANY},
  {.word = "new", L_NEW, 0},
  {.word = "nomask", L_TYPE_MODIFIER, NAME_NO_MASK},
  {.word = "object", L_BASIC_TYPE, TYPE_OBJECT},
  {.word = "parse_command", L_PARSE_COMMAND, 0},
  {.word = "private", L_TYPE_MODIFIER, NAME_PRIVATE},
  {.word = "protected", L_TYPE_MODIFIER, NAME_PROTECTED},
  {.word = "public", L_TYPE_MODIFIER, NAME_PUBLIC},
  {.word = "return", L_RETURN, 0},
  {.word = "sscanf", L_SSCANF, 0},
  {.word = "static", L_TYPE_MODIFIER, NAME_STATIC},
  {.word = "string", L_BASIC_TYPE, TYPE_STRING},
  {.word = "switch", L_SWITCH, 0},
  {.word = "time_expression", L_TIME_EXPRESSION, 0},
  {.word = "varargs", L_TYPE_MODIFIER, NAME_VARARGS},
  {.word = "void", L_BASIC_TYPE, TYPE_VOID},
  {.word = "while", L_WHILE, 0},
};

instr_t instrs[MAX_INSTRS];

#define TERM_ADD_INPUT 1
#define TERM_INCLUDE 2
#define TERM_START 4

typedef struct linked_buf_s
{
  struct linked_buf_s *prev;
  char term_type;
  char buf[DEFMAX];
  char *buf_end;
  char *outptr;
  char *last_nl;
}
linked_buf_t;

static linked_buf_t head_lbuf = { .prev = NULL, TERM_START };
static linked_buf_t *cur_lbuf;

static void handle_define (char *);
static void add_define (const char *, int, const char *); /* implementation in preprocess.c */
static void add_predefine (char *, int, char *);
static int expand_define (void);
static void add_input (const char *);
static int cond_get_exp (int);
static void inc_lexically_normal (const char* abs_base, const char *name, char *dest);
static void add_quoted_predefine (char *, char *);
static void lexerror (char *);
static int skip_to (char *, char *);
static void handle_cond (int);
static int inc_open (char *, const char *);
static void handle_include (const char *, int);
static int get_terminator (char *);
static int get_array_block (char *);
static int get_text_block (char *);
static void skip_line (void);
static void skip_comment (void);
static void deltrail (char *);
static void handle_pragma (char *);
static int cmygetc (void);
static void refill (void);
static void refill_buffer (void);
static int exgetc (void);
static int old_func (void);
static void yyerrorp (char *);

#define LEXER
#include "preprocess.c"

static void inc_lexically_normal (const char* abs_base, const char *name, char *dest) {
  char* slash;
  const char *from;

  strcpy (dest, abs_base);
  if ((slash = strrchr (dest, '/')))	/* strip filename */
    *slash = 0;
  else
    /* current_file was the file_name */
    /* include from the root directory */
    *dest = 0;

  from = name;
  while (*from == '/')
    {
      from++;
      *dest = 0;		/* absolute path */
    }

  /* process .. and . in the include header name */
  while (*from)
    {
      if (!strncmp (from, "../", 3))
        {
          if (*dest == 0)	/* including from above mudlib is NOT allowed */
            break;
          slash = strrchr (dest, '/');
          if (slash == NULL)	/* 1 component in dest */
            *dest = 0;
          else
            *slash = 0;
          from += 3;		/* skip "../" */
        }
      else if (!strncmp (from, "./", 2))
        {
          from += 2;
        }
      else
        {			/* append first component to dest */
          if (*dest)
            strcat (dest, "/");	/* only if dest is not empty !! */
          slash = strchr (from, '/');

          if (slash)
            {			/* from has 2 or more components */
              while (*from == '/')	/* find the start */
                from++;
              strncat (dest, from, slash - from);
              for (from = slash + 1; *from == '/'; from++);
            }
          else
            {
              /* this was the last component */
              strcat (dest, from);
              break;
            }
        }
    }
}

static void yyerrorp (char *s) {
  char buf[200];

  sprintf (buf, s, '#');
  yyerror (buf);
  lex_fatal++;
}

static void lexerror (char *s) {
  yyerror (s);
  lex_fatal++;
}

/**
 * @brief Skip lines until a specific preprocessor token in the input stream.
 * 
 * This function is used in preprocess.c to skip lines until a specific preprocessor
 * token is found, such as #endif or #else. It handles nested preprocessor directives
 * correctly.
 * 
 * @param token The token to skip to.
 * @param atoken An alternative token to skip to, or NULL if not needed.
 * @return 1 if `token` was found, 0 if `atoken`
 */
static int skip_to (char *token, char *atoken)
{
  char b[20], *p;
  char c;
  register char *yyp = outptr, *startp;
  char *b_end = b + 19;
  int nest;

  for (nest = 0;;)
    {
      /* check for preprocessor directives */
      if ((c = *yyp++) == '#')
        {
          while (is_wspace (c = *yyp++));
          startp = yyp - 1;
          for (p = b; !isspace (c) && c != LEX_EOF; c = *yyp++)
            {
              if (p < b_end)
                *p++ = c;
              else
                break;
            }
          *p = 0;
          if (!strcmp (b, "if") || !strcmp (b, "ifdef") || !strcmp (b, "ifndef"))
            {
              nest++;
            }
          else if (nest > 0)
            {
              if (!strcmp (b, "endif"))
                nest--;
            }
          else
            {
              if (!strcmp (b, token))
                {
                  outptr = startp;
                  *--outptr = '#';
                  return 1;
                }
              else if (atoken && !strcmp (b, atoken))
                {
                  outptr = startp;
                  *--outptr = '#';
                  return 0;
                }
              else if (!strcmp (b, "elif"))
                {
                  outptr = startp;
                  *--outptr = '#';
                  return (atoken == 0);
                }
            }
        }
      /* skip a line (or EOF) */
      while (c != '\n' && c != LEX_EOF)
        c = *yyp++;
      if (c == LEX_EOF)
        {
          lexerror ("Unexpected end of file while skipping");
          outptr = yyp - 1;
          return 1;
        }
      current_line++;
      total_lines++;
      if (yyp == last_nl + 1)
        {
          outptr = yyp;
          refill_buffer ();
          yyp = outptr;
        }
    }
}

/**
 * @brief Try to open an include file.
 * @param buf Buffer to store the normalized path.
 * @param name Argument of the #include directive.
 *             If it contains dot or dot-dot in the path, it is normalized using current_file as the base.
 * @return File descriptor, or -1 on failure.
 */
static int inc_open (char *buf, const char *name) {

  int i, fd;
  char *p;

  inc_lexically_normal (current_file, name, buf);
  if ((fd = FILE_OPEN (buf, O_RDONLY)) != -1)
    {
      opt_trace (TT_COMPILE|3, "opened (fd %d): \"%s\"", fd, buf);
      return fd;
    }
  /*
   * Search all include dirs specified.
   */
  for (p = strchr (name, '.'); p; p = strchr (p + 1, '.'))
    {
      if (p[1] == '.')
        return -1;
    }
  for (i = 0; i < inc_list_size; i++)
    {
      if (!inc_list)
        break;
      if (inc_list[i] == 0)
        continue;
      sprintf (buf, "%s/%s", inc_list[i], name);
      if ((fd = FILE_OPEN (buf, O_RDONLY)) != -1)
        {
          opt_trace (TT_COMPILE|3, "opened (fd %d): \"%s\"", fd, buf);
          return fd;
        }
    }
  return -1;
}

#define include_error(x) do {\
        current_line--;\
        yyerror(x);\
        current_line++;\
        } while(0)

static void handle_include (const char *inc_name, int optional) {
  char *p, *name;
  char fname[PATH_MAX];
  static char buf[1024];
  incstate_t *is;
  int delim, fd;

  /* need a writable copy */
  fname[sizeof(fname)-1] = 0;
  strncpy (fname, inc_name, sizeof(fname)-1);
  name = fname;
  opt_trace (TT_COMPILE|2, "header: %s", name);

  if (*name != '"' && *name != '<')
    {
      defn_t *d;

      if ((d = lookup_define (name)) && d->nargs == -1)
        {
          char *q;

          q = d->exps; /* #include MACRO */
          while (isspace (*q))
            q++;
          handle_include (q, optional);
        }
      else
        {
          include_error ("Missing leading \" or < in #include");
        }
      return;
    }

  /* convert "header" or <header> to header */
  delim = (*name++ == '"') ? '"' : '>';
  for (p = name; *p && *p != delim; p++);
  if (!*p)
    {
      include_error ("Missing trailing \" or > in #include");
      return;
    }

  if (strlen (name) > sizeof (buf) - 100)
    {
      include_error ("Include name too long");
      return;
    }
  *p = 0;

  if (++incnum == MAX_INCLUDE_DEPTH)
    {
      include_error ("Maximum include depth exceeded");
    }
  else if ((fd = inc_open (buf, name)) != -1) /* open header file */
    {
      is = ALLOCATE (incstate_t, TAG_COMPILER, "handle_include: 1");
      is->yyin_desc = yyin_desc;
      is->line = current_line;
      is->file = current_file;
      is->file_id = current_file_id;
      is->last_nl = last_nl;
      is->next = inctop;
      is->outptr = outptr;
      inctop = is; /* push new include state */
      current_line--;
      save_file_info (current_file_id, current_line - current_line_saved);
      current_line_base += current_line;
      current_line_saved = 0;
      current_line = 1;
      current_file = make_shared_string (buf);
      current_file_id = add_program_file (buf, 0);
      yyin_desc = fd;
      refill_buffer ();
    }
  else if (!optional)
    {
      sprintf (buf, "Cannot #include %s", name);
      include_error (buf);
    }
  else
    refill_buffer ();
}

static int get_terminator (char *terminator) {
  int c, j = 0;

  while (((c = *outptr++) != LEX_EOF) && (isalnum (c) || c == '_'))
    terminator[j++] = (char)c;

  terminator[j] = '\0';

  while (is_wspace (c) && c != LEX_EOF)
    c = *outptr++;

  if (c == LEX_EOF)
    return 0;

  if (c == '\n')
    {
      current_line++;
      if (outptr == last_nl + 1)
        refill_buffer ();
    }
  else
    {
      outptr--;
    }

  return j;
}

#define MAXCHUNK (MAXLINE*4)
#define NUMCHUNKS (DEFMAX/MAXCHUNK)

#define NEWCHUNK(line) \
    if (len == MAXCHUNK - 1) { \
        line[curchunk][MAXCHUNK - 1] = '\0'; \
        if (curchunk == NUMCHUNKS - 1) { \
            res = -2; \
            break; \
        } \
        line[++curchunk] = \
              (char *)DXALLOC(MAXCHUNK, TAG_COMPILER, "array/text chunk"); \
        len = 0; \
    }

static int get_array_block (char *term) {
  size_t termlen;			/* length of terminator */
  char *array_line[NUMCHUNKS];	/* allocate memory in chunks */
  int header, len;		/* header length; position in chunk */
  int startpos, startchunk;	/* start of line */
  int curchunk, res;		/* current chunk; this function's result */
  int c, i;			/* a char; an index counter */
  register char *yyp = outptr;

  /*
   * initialize
   */
  termlen = strlen (term);
  array_line[0] = (char *) DXALLOC (MAXCHUNK, TAG_COMPILER, "array_block");
  array_line[0][0] = '(';
  array_line[0][1] = '{';
  array_line[0][2] = '"';
  array_line[0][3] = '\0';
  header = 1;
  len = 3;
  startpos = 3;
  startchunk = 0;
  curchunk = 0;
  res = 0;

  while (1)
    {
      while (((c = *yyp++) != '\n') && (c != LEX_EOF))
        {
          NEWCHUNK (array_line);
          if (c == '"' || c == '\\')
            {
              array_line[curchunk][len++] = '\\';
              NEWCHUNK (array_line);
            }
          array_line[curchunk][len++] = (char)c;
        }

      if (c == '\n' && (yyp == last_nl + 1))
        {
          outptr = yyp;
          refill_buffer ();
          yyp = outptr;
        }

      /*
       * null terminate current chunk
       */
      array_line[curchunk][len] = '\0';

      if (res)
        {
          outptr = yyp;
          break;
        }

      /*
       * check for terminator
       */
      if ((!strncmp (array_line[startchunk] + startpos, term, termlen)) &&
          (!isalnum (*(array_line[startchunk] + startpos + termlen))) &&
          (*(array_line[startchunk] + startpos + termlen) != '_'))
        {
          /*
           * handle lone terminator on line
           */
          if (strlen (array_line[startchunk] + startpos) == (unsigned int)termlen)
            {
              current_line++;
              outptr = yyp;
            }
          else
            {
              /*
               * put back trailing portion after terminator
               */
              outptr = --yyp;	/* some operating systems give EOF only once */

              for (i = curchunk; i > startchunk; i--)
                add_input (array_line[i]);
              add_input (array_line[startchunk] + startpos + termlen);
            }

          /*
           * remove terminator from last chunk
           */
          array_line[startchunk][startpos - header] = '\0';

          /*
           * insert array block into input stream
           */
          *--outptr = ')';
          *--outptr = '}';
          for (i = startchunk; i >= 0; i--)
            add_input (array_line[i]);

          res = 1;
          break;
        }
      else
        {
          /*
           * only report end of file in array block, if not an include file
           */
          if (c == LEX_EOF && inctop == 0)
            {
              res = -1;
              outptr = yyp;
              break;
            }
          if (c == '\n')
            {
              current_line++;
            }
          /*
           * make sure there's room in the current chunk for terminator (ie
           * it's simpler if we don't have to deal with a terminator that
           * spans across chunks) fudge for "\",\"TERMINAL?\0", where '?'
           * is unknown
           */
          if (len + termlen + 5 > MAXCHUNK)
            {
              if (curchunk == NUMCHUNKS - 1)
                {
                  res = -2;
                  outptr = yyp;
                  break;
                }
              array_line[++curchunk] =
                (char *) DXALLOC (MAXCHUNK, TAG_COMPILER, "array_block");
              len = 0;
            }
          /*
           * header
           */
          array_line[curchunk][len++] = '"';
          array_line[curchunk][len++] = ',';
          array_line[curchunk][len++] = '"';
          array_line[curchunk][len] = '\0';

          startchunk = curchunk;
          startpos = len;
          header = 2;
        }
    }

  /*
   * free chunks
   */
  for (i = curchunk; i >= 0; i--)
    FREE (array_line[i]);

  return res;
}

static int get_text_block (char *term) {
  size_t termlen;			/* length of terminator */
  char *text_line[NUMCHUNKS];	/* allocate memory in chunks */
  int len;			/* position in chunk */
  int startpos, startchunk;	/* start of line */
  int curchunk, res;		/* current chunk; this function's result */
  int c, i;			/* a char; an index counter */
  register char *yyp = outptr;

  /*
   * initialize
   */
  termlen = strlen (term);
  text_line[0] = (char *) DXALLOC (MAXCHUNK, TAG_COMPILER, "text_block");
  text_line[0][0] = '"';
  text_line[0][1] = '\0';
  len = 1;
  startpos = 1;
  startchunk = 0;
  curchunk = 0;
  res = 0;

  while (1)
    {
      while (((c = *yyp++) != '\n') && (c != LEX_EOF))
        {
          NEWCHUNK (text_line);
          if (c == '"' || c == '\\')
            {
              text_line[curchunk][len++] = '\\';
              NEWCHUNK (text_line);
            }
          text_line[curchunk][len++] = (char)c;
        }

      if (c == '\n' && yyp == last_nl + 1)
        {
          outptr = yyp;
          refill_buffer ();
          yyp = outptr;
        }

      /*
       * null terminate current chunk
       */
      text_line[curchunk][len] = '\0';

      if (res)
        {
          outptr = yyp;
          break;
        }

      /*
       * check for terminator
       */
      if ((!strncmp (text_line[startchunk] + startpos, term, termlen)) &&
          (!isalnum (*(text_line[startchunk] + startpos + termlen))) &&
          (*(text_line[startchunk] + startpos + termlen) != '_'))
        {
          if (strlen (text_line[startchunk] + startpos) == (unsigned int)termlen)
            {
              current_line++;
              outptr = yyp;
            }
          else
            {
              char *p, *q;
              /*
               * put back trailing portion after terminator
               */
              outptr = --yyp;	/* some operating systems give EOF only once */

              for (i = curchunk; i > startchunk; i--)
                {
                  /* Ick.  go back and unprotect " and \ */
                  p = text_line[i];
                  while (*p && *p != '\\')
                    p++;
                  if (*p)
                    {
                      q = p++;
                      do
                        {
                          *q++ = *p++;
                          if (*p == '\\')
                            p++;
                        }
                      while (*p);
                      *q = 0;
                    }

                  add_input (text_line[i]);
                }
              p = text_line[startchunk] + startpos + termlen;
              while (*p && *p != '\\')
                p++;
              if (*p)
                {
                  q = p++;
                  do
                    {
                      *q++ = *p++;
                      if (*p == '\\')
                        p++;
                    }
                  while (*p);
                  *q = 0;
                }
              add_input (text_line[startchunk] + startpos + termlen);
            }

          /*
           * remove terminator from last chunk
           */
          text_line[startchunk][startpos] = '\0';

          /*
           * insert text block into input stream
           */
          *--outptr = '\0';
          *--outptr = '"';

          for (i = startchunk; i >= 0; i--)
            add_input (text_line[i]);

          res = 1;
          break;
        }
      else
        {
          /*
           * only report end of file in text block, if not an include file
           */
          if (c == LEX_EOF && inctop == 0)
            {
              res = -1;
              outptr = yyp;
              break;
            }
          if (c == '\n')
            {
              current_line++;
            }
          /*
           * make sure there's room in the current chunk for terminator (ie
           * it's simpler if we don't have to deal with a terminator that
           * spans across chunks) fudge for "\\nTERMINAL?\0", where '?' is
           * unknown
           */
          if (len + termlen + 4 > MAXCHUNK)
            {
              if (curchunk == NUMCHUNKS - 1)
                {
                  res = -2;
                  outptr = yyp;
                  break;
                }
              text_line[++curchunk] =
                (char *) DXALLOC (MAXCHUNK, TAG_COMPILER, "text_block");
              len = 0;
            }
          /*
           * header
           */
          text_line[curchunk][len++] = '\\';
          text_line[curchunk][len++] = 'n';
          text_line[curchunk][len] = '\0';

          startchunk = curchunk;
          startpos = len;
        }
    }

  /*
   * free chunks
   */
  for (i = curchunk; i >= 0; i--)
    FREE (text_line[i]);

  return res;
}

static void skip_line () {
  int c;
  register char *yyp = outptr;

  while (((c = *yyp++) != '\n') && (c != LEX_EOF));

  /* Next read of this '\n' will do refill_buffer() if neccesary */
  if (c == '\n')
    yyp--;
  outptr = yyp;
}

static void skip_comment () {
  int c = '*';
  register char *yyp = outptr;

  for (;;)
    {
      while ((c = *yyp++) != '*')
        {
          if (c == LEX_EOF)
            {
              outptr = --yyp;
              lexerror ("End of file in a comment");
              return;
            }
          if (c == '\n')
            {
              nexpands = 0;
              current_line++;
              if (yyp == last_nl + 1)
                {
                  outptr = yyp;
                  refill_buffer ();
                  yyp = outptr;
                }
            }
        }
      if (*(yyp - 2) == '/')
        yywarn ("/* found in comment.");
      do
        {
          if ((c = *yyp++) == '/')
            {
              outptr = yyp;
              return;
            }
          if (c == '\n')
            {
              nexpands = 0;
              current_line++;
              if (yyp == last_nl + 1)
                {
                  outptr = yyp;
                  refill_buffer ();
                  yyp = outptr;
                }
            }
        }
      while (c == '*');
    }
}

static void deltrail (char *str) {
  char *p;

  p = str;
  if (!*p)
    {
      lexerror ("Illegal # command");
    }
  else
    {
      while (*p && !isspace (*p))
        p++;
      *p = 0;
    }
}

#define SAVEC \
    if (yyp < yytext+MAXLINE-5)\
       *yyp++ = c;\
    else {\
       lexerror("Line too long");\
       break;\
    }

typedef struct pragma_s {
  char *name;
  int value;
}
pragma_t;

static pragma_t our_pragmas[] = {
  {"strict_types", PRAGMA_STRICT_TYPES},
  {"save_types", PRAGMA_SAVE_TYPES},
  {"save_binary", PRAGMA_SAVE_BINARY},
  {"warnings", PRAGMA_WARNINGS},
  {"optimize", 0},
  {"show_error_context", PRAGMA_ERROR_CONTEXT},
  {0, 0}
};

static void handle_pragma (char *str) {
  int i;
  int no_flag;

  if (strncmp (str, "no_", 3) == 0)
    {
      str += 3;
      no_flag = 1;
    }
  else
    no_flag = 0;

  for (i = 0; our_pragmas[i].name; i++)
    {
      if (strcmp (our_pragmas[i].name, str) == 0)
        {
          if (no_flag)
            {
              pragmas &= ~our_pragmas[i].value;
            }
          else
            {
              pragmas |= our_pragmas[i].value;
            }
          return;
        }
    }
  yywarn ("Unknown #pragma, ignored.");
}

char* show_error_context () {
  static char buf[60];
  extern int yychar;
  char sub_context[25];
  register char *yyp, *yyp2;
  int len;

  if (yychar == -1)
    strcpy (buf, " around ");
  else
    strcpy (buf, " before ");
  yyp = outptr;
  yyp2 = sub_context;
  len = 20;
  while (len--)
    {
      if (*yyp == '\n')
        {
          if (len == 19)
            strcat (buf, "the end of line");
          break;
        }
      else if (*yyp == LEX_EOF)
        {
          if (len == 19)
            strcat (buf, "the end of file");
          break;
        }
      *yyp2++ = *yyp++;
    }
  *yyp2 = 0;
  if (yyp2 != sub_context)
    strcat (buf, sub_context);
  strcat (buf, "\n");
  return buf;
}

static void refill_buffer () {
  if (cur_lbuf != &head_lbuf)
    {
      if (outptr >= cur_lbuf->buf_end && cur_lbuf->term_type == TERM_ADD_INPUT)
        {
          /* In this case it cur_lbuf cannot have been allocated due to #include */
          linked_buf_t *prev_lbuf = cur_lbuf->prev;

          FREE (cur_lbuf);
          cur_lbuf = prev_lbuf;
          outptr = cur_lbuf->outptr;
          last_nl = cur_lbuf->last_nl;
          if (cur_lbuf->term_type == TERM_ADD_INPUT || (outptr != last_nl + 1))
            return;
        }
    }

  /* Here we are sure that we need more from the file */
  /* Assume outptr is one beyond a newline at last_nl */
  /* or after an #include .... */

  {
    char *end;
    char *p;
    size_t size;

    if (!inctop)
      {
        /* First check if there's enough space at the end */
        end = cur_lbuf->buf + DEFMAX;
        if (end - cur_lbuf->buf_end > MAXLINE + 5)
          {
            p = cur_lbuf->buf_end;
          }
        else
          {
            /* No more space at the end */
            size = cur_lbuf->buf_end - outptr + 1;	/* Include newline */
            memcpy (outptr - MAXLINE - 1, outptr - 1, size);
            outptr -= MAXLINE;
            p = outptr + size - 1;
          }

        if (yyin_desc != -1)
          {
            unsigned int max_read = MAXLINE;
            for (size = 0; max_read > 0; )
              {
                int n = FILE_READ (yyin_desc, p + size, max_read);
                if (n <= 0)
                  break;
                size += n;
                max_read -= n;
              }
            cur_lbuf->buf_end = p += size;
          }
        else
          {
            /* [NEOLITH-EXTENSION] If the file descriptor is -1, there is no actual file to read. */
            size = 0;
          }
        if (size < MAXLINE) /* EOF? */
          {
            *(last_nl = p) = LEX_EOF;
            return;
          }
        while (*--p != '\n');
        if (p == outptr - 1)
          {
            lexerror ("Line too long");
            *(last_nl = cur_lbuf->buf_end - 1) = '\n';
            return;
          }
        last_nl = p; /* last newline */
        return;
      }
    else
      {
        int flag = 0;

        /* We are reading from an include file */
        /* Is there enough space? */
        end = inctop->outptr;

        /* See if we are the last include in a different linked buffer */
        if (cur_lbuf->term_type == TERM_INCLUDE &&
            !(end >= cur_lbuf->buf && end < cur_lbuf->buf + DEFMAX))
          {
            end = cur_lbuf->buf_end;
            flag = 1;
          }

        size = end - outptr + 1;	/* Include newline */
        if (outptr - cur_lbuf->buf > 2 * MAXLINE)
          {
            memcpy (outptr - MAXLINE - 1, outptr - 1, size);
            outptr -= MAXLINE;
            p = outptr + size - 1;
          }
        else
          {			/* No space, need to allocate new buffer */
            linked_buf_t *new_lbuf;
            char *new_outp;

            if (!(new_lbuf = ALLOCATE (linked_buf_t, TAG_COMPILER, "refill_bufer")))
              {
                lexerror ("Out of memory when allocating new buffer\n");
                return;
              }
            cur_lbuf->last_nl = last_nl;
            cur_lbuf->outptr = outptr;
            new_lbuf->prev = cur_lbuf;
            new_lbuf->term_type = TERM_INCLUDE;
            new_outp = new_lbuf->buf + DEFMAX - MAXLINE - size - 5;
            memcpy (new_outp - 1, outptr - 1, size);
            cur_lbuf = new_lbuf;
            outptr = new_outp;
            p = outptr + size - 1;
            flag = 1;
          }

        if (yyin_desc != -1)
          {
            unsigned int max_read = MAXLINE;
            for (size = 0; max_read > 0; )
              {
                int n = FILE_READ (yyin_desc, p + size, max_read);
                if (n <= 0)
                  break;
                size += n;
                max_read -= n;
              }
            end = p += size;
            if (flag)
              cur_lbuf->buf_end = p;
          }
        else
          {
            size = 0;
          }
        if (size < MAXLINE)
          {
            *(last_nl = p) = LEX_EOF;
            return;
          }
        while (*--p != '\n');
        if (p == outptr - 1)
          {
            lexerror ("Line too long.");
            *(last_nl = end - 1) = '\n';
            return;
          }
        last_nl = p;
        return;
      }
  }
}

static int function_flag = 0;

void push_function_context () {
  function_context_t *fc;
  parse_node_t *node;

  if (last_function_context == MAX_FUNCTION_DEPTH - 1)
    {
      yyerror ("Function pointers nested too deep");
      return;
    }
  fc = &function_context_stack[++last_function_context];
  fc->num_parameters = 0;
  fc->num_locals = 0;
  node = new_node_no_line ();
  node->l.expr = node;
  node->r.expr = 0;
  node->kind = 0;
  fc->values_list = node;
  fc->bindable = 0;
  fc->parent = current_function_context;

  current_function_context = fc;
}

void pop_function_context () {
  current_function_context = current_function_context->parent;
  last_function_context--;
}

static int old_func () {
  add_input (yytext);
  push_function_context ();
  return L_FUNCTION_OPEN;
}

static void ensure_valid_wide_string(const char* mbs) {
  if (mbstowcs(NULL, mbs, 0) == (size_t)-1)
    lexerror("Invalid wide string literal.");
}

#define return_assign(opcode) { yylval.number = opcode; return L_ASSIGN; }
#define return_order(opcode) { yylval.number = opcode; return L_ORDER; }

static int wide_char_literal = 0;

/**
 * @brief The lexical analyzer.
 * @return The next token, or -1 on end of file or fatal error.
 */
int yylex () {
  static char partial[MAXLINE + 5];	/* extra 5 for safety buffer */
  static char terminator[MAXLINE + 5];
  int is_float;
  double myreal;
  char *partp;

  register char *yyp;
  register char c;

  yytext[0] = 0;

  partp = partial;
  partial[0] = 0;

  for (;;)
    {
      if (lex_fatal)
        {
          return -1;
        }
      switch (c = *outptr++)
        {
        case LEX_EOF:
          if (inctop)
            {
              incstate_t *p;

              p = inctop;
              close (yyin_desc);
              opt_trace (TT_COMPILE|3, "closed (fd %d): \"%s\"", yyin_desc,
                         current_file ? current_file : "<unknown>");
              save_file_info (current_file_id, current_line - current_line_saved);
              current_line_saved = p->line - 1;
              /* add the lines from this file, and readjust to be relative
                 to the file we're returning to */
              current_line_base += current_line - current_line_saved;
              free_string (current_file);
              nexpands = 0;
              if (outptr >= cur_lbuf->buf_end)
                {
                  linked_buf_t *prev_lbuf;
                  if ((prev_lbuf = cur_lbuf->prev))
                    {
                      FREE (cur_lbuf);
                      cur_lbuf = prev_lbuf;
                    }
                }

              current_file = p->file;
              current_file_id = p->file_id;
              current_line = p->line;

              yyin_desc = p->yyin_desc;
              last_nl = p->last_nl;
              outptr = p->outptr;
              inctop = p->next;
              incnum--;
              FREE ((char *) p);
              outptr[-1] = '\n';
              if (outptr == last_nl + 1)
                refill_buffer ();
              break;
            }
          if (iftop)
            {
              ifstate_t *p = iftop;

              yyerror (p->state == EXPECT_ENDIF ? "Missing #endif" : "Missing #else/#elif");
              while (iftop)
                {
                  p = iftop;
                  iftop = p->next;
                  FREE ((char *) p);
                }
            }
          outptr--;
          return -1;
        case '\n':
          {
            nexpands = 0;
            current_line++;
            total_lines++;
            if (outptr == last_nl + 1)
              refill_buffer ();
          }
        case ' ':
        case '\t':
        case '\f':
        case '\v':
        case '\r':
          break;
        case '+':
          {
            switch (*outptr++)
              {
              case '+':
                return L_INC;
              case '=':
                return_assign (F_ADD_EQ);
              default:
                outptr--;
                return '+';
              }
          }
        case '-':
          {
            switch (*outptr++)
              {
              case '>':
                return L_ARROW;
              case '-':
                return L_DEC;
              case '=':
                return_assign (F_SUB_EQ);
              default:
                outptr--;
                return '-';
              }
          }
        case '&':
          {
            switch (*outptr++)
              {
              case '&':
                return L_LAND;
              case '=':
                return_assign (F_AND_EQ);
              default:
                outptr--;
                return '&';
              }
          }
        case '|':
          {
            switch (*outptr++)
              {
              case '|':
                return L_LOR;
              case '=':
                return_assign (F_OR_EQ);
              default:
                outptr--;
                return '|';
              }
          }
        case '^':
          {
            if (*outptr++ == '=')
              return_assign (F_XOR_EQ);
            outptr--;
            return '^';
          }
        case '<':
          {
            switch (*outptr++)
              {
              case '<':
                {
                  if (*outptr++ == '=')
                    return_assign (F_LSH_EQ);
                  outptr--;
                  return L_LSH;
                }
              case '=':
                return_order (F_LE);
              default:
                outptr--;
                return '<';
              }
          }
        case '>':
          {
            switch (*outptr++)
              {
              case '>':
                {
                  if (*outptr++ == '=')
                    return_assign (F_RSH_EQ);
                  outptr--;
                  return L_RSH;
                }
              case '=':
                return_order (F_GE);
              default:
                outptr--;
                return_order (F_GT);
              }
          }
        case '*':
          {
            if (*outptr++ == '=')
              return_assign (F_MULT_EQ);
            outptr--;
            return '*';
          }
        case '%':
          {
            if (*outptr++ == '=')
              return_assign (F_MOD_EQ);
            outptr--;
            return '%';
          }
        case '/':
          switch (*outptr++)
            {
            case '*':
              skip_comment ();
              break;
            case '/':
              skip_line ();
              break;
            case '=':
              return_assign (F_DIV_EQ);
            default:
              outptr--;
              return '/';
            }
          break;
        case '=':
          if (*outptr++ == '=')
            return L_EQ;
          outptr--;
          yylval.number = F_ASSIGN;
          return L_ASSIGN;
        case '(':
          yyp = outptr;
          while (isspace (c = *yyp++))
            {
              if (c == '\n')
                {
                  current_line++;
                  if (yyp == last_nl + 1)
                    {
                      outptr = yyp;
                      refill_buffer ();
                      yyp = outptr;
                    }
                }
            }

          switch (c)
            {
            case '{':
              {
                outptr = yyp;
                return L_ARRAY_OPEN;
              }
            case '[':
              {
                outptr = yyp;
                return L_MAPPING_OPEN;
              }
            case ':':
              {
                if ((c = *yyp++) == ':')
                  {
                    outptr = yyp -= 2;
                    return '(';
                  }
                else
                  {
                    while (isspace (c))
                      {
                        if (c == '\n')
                          {
                            if (yyp == last_nl + 1)
                              {
                                outptr = yyp;
                                refill_buffer ();
                                yyp = outptr;
                              }
                            current_line++;
                          }
                        c = *yyp++;
                      }

                    outptr = yyp;

                    if (isalpha (c) || c == '_')
                      {
                        function_flag = 1;
                        goto parse_identifier;
                      }

                    outptr--;
                    push_function_context ();
                    return L_FUNCTION_OPEN;
                  }

              }
            default:
              {
                outptr = yyp - 1;
                return '(';
              }
            }

        case '$':
          if (!current_function_context)
            {
              yyerror ("$var illegal outside of function pointer");
              return '$';
            }
          if (current_function_context->num_parameters == -2)
            {
              yyerror ("$var illegal inside anonymous function pointer");
              return '$';
            }
          else
            {
              if (!isdigit (c = *outptr++))
                {
                  outptr--;
                  return '$';
                }
              yyp = yytext;
              SAVEC;
              for (;;)
                {
                  if (!isdigit (c = *outptr++))
                    break;
                  SAVEC;
                }
              outptr--;
              *yyp = 0;
              yylval.number = atoi (yytext) - 1;
              if (yylval.number < 0)
                yyerror ("In function parameter $num, num must be >= 1");
              else if (yylval.number > 255)
                yyerror ("only 256 parameters allowed");
              else if (yylval.number >= current_function_context->num_parameters)
                current_function_context->num_parameters = (short)(yylval.number + 1);
              return L_PARAMETER;
            }
        case ')':
        case '{':
        case '}':
        case '[':
        case ']':
        case ';':
        case ',':
        case '~':
        case '?':
          return c;
        case '!':
          if (*outptr++ == '=')
            return L_NE;
          outptr--;
          return L_NOT;
        case ':':
          if (*outptr++ == ':')
            return L_COLON_COLON;
          outptr--;
          return ':';
        case '.':
          if (*outptr++ == '.')
            {
              if (*outptr++ == '.')
                return L_DOT_DOT_DOT;
              outptr--;
              return L_RANGE;
            }
          outptr--;
          goto badlex;
        case '#':
          if (*(outptr - 2) == '\n')
            {
              char *arg = 0;
              int quote;

              while (is_wspace (c = *outptr++));

              yyp = yytext;

              for (quote = 0;;)
                {

                  if (c == '"')
                    quote ^= 1;
                  else if (c == '/' && !quote)
                    {
                      if (*outptr == '*')
                        {
                          outptr++;
                          skip_comment ();
                          c = *outptr++;
                        }
                      else if (*outptr == '/')
                        {
                          outptr++;
                          skip_line ();
                          c = *outptr++;
                        }
                    }
                  if (!arg && isspace (c))
                    arg = yyp;
                  if (c == '\n' || c == LEX_EOF)
                    break;
                  SAVEC;
                  c = *outptr++;
                }
              if (arg)
                {
                  *arg++ = 0;
                  while (isspace (*arg))
                    arg++;
                }
              else
                {
                  arg = yyp;
                }
              *yyp = 0;
              if (!strcmp ("include", yytext))
                {
                  current_line++;
                  if (c == LEX_EOF)
                    {
                      *(last_nl = --outptr) = LEX_EOF;
                      outptr[-1] = '\n';
                    }
                  handle_include (arg, 0);
                  break;
                }
              else
                {
                  if (outptr == last_nl + 1)
                    refill_buffer ();

                  if (strcmp ("define", yytext) == 0)
                    {
                      handle_define (arg);
                    }
                  else if (strcmp ("if", yytext) == 0)
                    {
                      int cond;

                      *--outptr = '\0';
                      add_input (arg);
                      cond = cond_get_exp (0);
                      if (*outptr++)
                        {
                          yyerror ("Condition too complex in #if");
                          while (*outptr++);
                        }
                      else
                        handle_cond (cond);
                    }
                  else if (strcmp ("ifdef", yytext) == 0)
                    {
                      deltrail (arg);
                      handle_cond (lookup_define (arg) != 0);
                    }
                  else if (strcmp ("ifndef", yytext) == 0)
                    {
                      deltrail (arg);
                      handle_cond (lookup_define (arg) == 0);
                    }
                  else if (strcmp ("elif", yytext) == 0)
                    {
                      handle_elif (arg);
                    }
                  else if (strcmp ("else", yytext) == 0)
                    {
                      handle_else ();
                    }
                  else if (strcmp ("endif", yytext) == 0)
                    {
                      handle_endif ();
                    }
                  else if (strcmp ("undef", yytext) == 0)
                    {
                      defn_t *d;

                      deltrail (arg);
                      if ((d = lookup_define (arg)))
                        {
                          if (d->flags & DEF_IS_PREDEF)
                            yyerror ("Illegal to #undef a predefined value.");
                          else
                            d->flags |= DEF_IS_UNDEFINED;
                        }
                    }
                  else if (strcmp ("echo", yytext) == 0)
                    {
                      debug_message ("{}\t%s", arg);
                    }
                  else if (strcmp ("pragma", yytext) == 0)
                    {
                      handle_pragma (arg);
                    }
                  else
                    {
                      yyerror ("Unrecognised # directive");
                    }
                  *--outptr = '\n';
                  break;
                }
            }
          else
            goto badlex;
        case 'L':
          /* wide character literal, see https://en.cppreference.com/w/cpp/language/character_literal.html */
          if (*outptr == '"')
            {
              wide_char_literal = 1;
              c = *outptr++;
              goto case_string;
            }
          else if (*outptr != '\'')
            {
              wide_char_literal = 0;
              goto parse_identifier;
            }
          wide_char_literal = 1;
          outptr++;
          /* fall through */
        case '\'':

          if (*outptr++ == '\\')
            {
              switch (*outptr++)
                {
                case 'n':
                  yylval.number = '\n';
                  break;
                case 't':
                  yylval.number = '\t';
                  break;
                case 'r':
                  yylval.number = '\r';
                  break;
                case 'b':
                  yylval.number = '\b';
                  break;
                case 'a':
                  yylval.number = '\x07';
                  break;
                case 'e':
                  yylval.number = '\x1b';
                  break;
                case '\'':
                  yylval.number = '\'';
                  break;
                case '\"':
                  yylval.number = '\"';
                  break;
                case '\\':
                  yylval.number = '\\';
                  break;
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
                  outptr--;
                  yylval.number = strtol (outptr, &outptr, 8);
                  if (yylval.number > 255)
                    {
                      yywarn ("Illegal character constant");
                      yylval.number = 'x';
                    }
                  break;
                case 'x':
                  if (!isxdigit (*outptr))
                    {
                      yylval.number = 'x';
                      yywarn ("\\x must be followed by a valid hex value; interpreting as 'x' instead.");
                    }
                  else
                    {
                      yylval.number = strtol (outptr, &outptr, 16);
                      if (yylval.number > 255)
                        {
                          yywarn ("Illegal character constant");
                          yylval.number = 'x';
                        }
                    }
                  break;
                case '\n':
                  yylval.number = '\n';
                  current_line++;
                  total_lines++;
                  if ((outptr = last_nl + 1))
                    refill_buffer ();
                  break;
                default:
                  yywarn ("Unknown \\ escape");
                  yylval.number = *(outptr - 1);
                  break;
                }
            }
          else
            {
              if (wide_char_literal)
                {
                  /* For wide char literals, we accept multi-byte characters */
                  wchar_t wc = 0;
                  int bytes = mblen (outptr - 1, MB_CUR_MAX);
                  if (bytes < 1)
                    {
                      yywarn ("Illegal wide character constant");
                      yylval.number = 'x';
                    }
                  else
                    {
                      if (bytes != mbtowc (&wc, outptr - 1, bytes))
                        {
                          yywarn ("Illegal wide character constant");
                          yylval.number = 'x';
                        }
                      else
                        {
                          yylval.number = wc;
                        }
                      outptr += bytes - 1;
                    }
                }
              else
                {
                  /* ordinary single character literal */
                  yylval.number = *(outptr - 1);
                }
            }

          if (*outptr++ != '\'')
            {
              outptr--;
              yyerror ("Illegal character constant");
              yylval.number = 0;
            }
          return L_NUMBER;
        case '@':
          {
            int rc;
            int tmp;

            if ((tmp = *outptr++) != '@')
              {
                /* check for Robocoder's @@ block */
                outptr--;
              }
            if (!get_terminator (terminator))
              {
                lexerror ("Illegal terminator");
                break;
              }
            if (tmp == '@')
              {
                rc = get_array_block (terminator);

                if (rc > 0)
                  {
                    /* outptr is pointing at "({" for histortical reasons */
                    outptr += 2;
                    return L_ARRAY_OPEN;
                  }
                else if (rc == -1)
                  {
                    lexerror ("End of file in array block");
                    return LEX_EOF;
                  }
                else
                  {		/* if rc == -2 */
                    yyerror ("Array block exceeded maximum length");
                  }
              }
            else
              {
                rc = get_text_block (terminator);

                if (rc > 0)
                  {
                    int n;

                    /*
                     * make string token and clean up
                     */
                    yylval.string = scratch_copy_string (outptr);

                    n = (int)strlen (outptr) + 1;
                    outptr += n;

                    return L_STRING;
                  }
                else if (rc == -1)
                  {
                    lexerror ("End of file in text block");
                    return LEX_EOF;
                  }
                else
                  {		/* if (rc == -2) */
                    yyerror ("Text block exceeded maximum length");
                  }
              }
          }
          break;
        case '"':
          {
            ptrdiff_t l;
            register unsigned char *to;

case_string:
            to = scr_tail + 1; /* scratchpad.c */
            if ((l = scratch_end - 1 - to) > 255)
              l = 255;
            while (l-- > 0)
              {
                switch (c = *outptr++)
                  {
                  case LEX_EOF:
                    lexerror ("End of file in string");
                    return LEX_EOF;

                  case '"':
                    scr_last = scr_tail + 1;
                    *to++ = 0;
                    scr_tail = to;
                    *to = (unsigned char)(to - scr_last);
                    yylval.string = (char *) scr_last;
                    if (wide_char_literal)
                      ensure_valid_wide_string (yylval.string);
                    return L_STRING;

                  case '\n':
                    current_line++;
                    total_lines++;
                    if (outptr == last_nl + 1)
                      refill_buffer ();
                    *to++ = '\n';
                    break;

                  case '\\':
                    /* Don't copy the \ in yet */
                    switch (*outptr++)
                      {
                      case '\n':
                        current_line++;
                        total_lines++;
                        if (outptr == last_nl + 1)
                          refill_buffer ();
                        l++;	/* Nothing is copied */
                        break;
                      case LEX_EOF:
                        lexerror ("End of file in string");
                        return LEX_EOF;
                      case 'n':
                        *to++ = '\n';
                        break;
                      case 't':
                        *to++ = '\t';
                        break;
                      case 'r':
                        *to++ = '\r';
                        break;
                      case 'b':
                        *to++ = '\b';
                        break;
                      case 'a':
                        *to++ = '\x07';
                        break;
                      case 'e':
                        *to++ = '\x1b';
                        break;
                      case '"':
                        *to++ = '"';
                        break;
                      case '\\':
                        *to++ = '\\';
                        break;
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
                        {
                          int tmp;
                          outptr--;
                          tmp = strtol (outptr, &outptr, 8);
                          if (tmp > 255)
                            {
                              yywarn ("Illegal character constant in string.");
                              tmp = 'x';
                            }
                          *to++ = (unsigned char)tmp;
                          break;
                        }
                      case 'x':
                        {
                          int tmp;
                          if (!isxdigit (*outptr))
                            {
                              *to++ = 'x';
                              yywarn ("\\x must be followed by a valid hex value; interpreting as 'x' instead.");
                            }
                          else
                            {
                              tmp = strtol (outptr, &outptr, 16);
                              if (tmp > 255)
                                {
                                  yywarn ("Illegal character constant.");
                                  tmp = 'x';
                                }
                              *to++ = (unsigned char)tmp;
                            }
                          break;
                        }
                      default:
                        /* Add backslash as well */
                        *to++ = '\\';
                        *to++ = *(outptr - 1);
                        yywarn("Unknown \\ escape.");
                      }
                    break;
                  default:
                    *to++ = c;
                  }
              }

            /* Not enough space, we now copy the rest into yytext */
            l = MAXLINE - (to - scr_tail);

            yyp = yytext;
            while (l--)
              {
                switch (c = *outptr++)
                  {
                  case LEX_EOF:
                    lexerror ("End of file in string");
                    return LEX_EOF;

                  case '"':
                    {
                      char *res;
                      *yyp++ = '\0';
                      res = scratch_large_alloc ((yyp - yytext) + (to - scr_tail) - 1);
                      strncpy (res, (char *) (scr_tail + 1), (to - scr_tail) - 1);
                      strcpy (res + (to - scr_tail) - 1, yytext);
                      yylval.string = res;
                      if (wide_char_literal)
                        ensure_valid_wide_string (yylval.string);
                      return L_STRING;
                    }

                  case '\n':
                    current_line++;
                    total_lines++;
                    if (outptr == last_nl + 1)
                      refill_buffer ();
                    *yyp++ = '\n';
                    break;

                  case '\\':
                    /* Don't copy the \ in yet */
                    switch (*outptr++)
                      {
                      case '\n':
                        current_line++;
                        total_lines++;
                        if (outptr == last_nl + 1)
                          refill_buffer ();
                        l++;	/* Nothing is copied */
                        break;
                      case LEX_EOF:
                        lexerror ("End of file in string");
                        return LEX_EOF;
                      case 'n':
                        *yyp++ = '\n';
                        break;
                      case 't':
                        *yyp++ = '\t';
                        break;
                      case 'r':
                        *yyp++ = '\r';
                        break;
                      case 'b':
                        *yyp++ = '\b';
                        break;
                      case 'a':
                        *yyp++ = '\x07';
                        break;
                      case 'e':
                        *yyp++ = '\x1b';
                        break;
                      case '"':
                        *yyp++ = '"';
                        break;
                      case '\\':
                        *yyp++ = '\\';
                        break;
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
                        {
                          int tmp;
                          outptr--;
                          tmp = strtol (outptr, &outptr, 8);
                          if (tmp > 255)
                            {
                              yywarn ("Illegal character constant in string.");
                              tmp = 'x';
                            }
                          *yyp++ = (char)tmp;
                          break;
                        }
                      case 'x':
                        {
                          int tmp;
                          if (!isxdigit (*outptr))
                            {
                              *yyp++ = 'x';
                              yywarn ("\\x must be followed by a valid hex value; interpreting as 'x' instead.");
                            }
                          else
                            {
                              tmp = strtol (outptr, &outptr, 16);
                              if (tmp > 255)
                                {
                                  yywarn ("Illegal character constant.");
                                  tmp = 'x';
                                }
                              *yyp++ = (char)tmp;
                            }
                          break;
                        }
                      default:
                        *yyp++ = '\\';
                        *yyp++ = *(outptr - 1);
                      }
                    break;

                  default:
                    *yyp++ = c;
                  }
              }

            /* Not even enough length, declare too long string error */
            lexerror ("String too long");
            *yyp++ = '\0';
            {
              char *res;
              res = scratch_large_alloc ((yyp - yytext) + (to - scr_tail) - 1);
              strncpy (res, (char *) (scr_tail + 1), (to - scr_tail) - 1);
              strcpy (res + (to - scr_tail) - 1, yytext);
              yylval.string = res;
              if (wide_char_literal)
                ensure_valid_wide_string (yylval.string);
              return L_STRING;
            }
          }
        case '0':
          c = *outptr++;
          if (c == 'X' || c == 'x')
            {
              yyp = yytext;
              for (;;)
                {
                  c = *outptr++;
                  SAVEC;
                  if (!isxdigit (c))
                    break;
                }
              outptr--;
              yylval.number = strtoll (yytext, (char **) NULL, 0x10);
              return L_NUMBER;
            }
          outptr--;
          c = '0';
          /* fall through */
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          is_float = 0;
          yyp = yytext;
          *yyp++ = c;
          for (;;)
            {
              c = *outptr++;
              if (c == '.')
                {
                  if (!is_float)
                    {
                      is_float = 1;
                    }
                  else
                    {
                      is_float = 0;
                      outptr--;
                      break;
                    }
                }
              else if (c == 'e' || c == 'E')
                {
                  is_float = 1;
                  SAVEC;
                  c = *outptr++;
                  if (c == '+' || c == '-')
                    {
                      SAVEC;
                    }
                  else
                    {
                      outptr--;
                    }
                  continue;
                }
              else if (c == 'f' || c == 'F' || c == 'd' || c == 'D') /* optional 'f' or 'd' suffix */
                {
                  is_float = 1;
                  SAVEC;
                  c = *outptr++;
                  break;
                }
              else if (!isdigit (c))
                break;
              SAVEC;
            }
          outptr--;
          *yyp = 0;
          if (is_float)
            {
              sscanf (yytext, "%lf", &myreal);
              yylval.real = myreal;
              return L_REAL;
            }
          else
            {
              yylval.number = strtoll (yytext, NULL, 10);
              return L_NUMBER;
            }
        default:
          if (isalpha (c) || c == '_') /* identifier */
            {
              size_t r;

parse_identifier:
              yyp = yytext;
              *yyp++ = c;
              for (;;)
                {
                  if (!isalnum (c = *outptr++) && (c != '_'))
                    break;
                  SAVEC;
                }
              *yyp = 0;
              if (c == '#')
                {
                  if (*outptr++ != '#')
                    lexerror ("Single '#' in identifier -- use '##' for token pasting");
                  outptr -= 2;
                  if (!expand_define ())
                    {
                      if (partp + (r = strlen (yytext)) +
                          (function_flag ? 3 : 0) - partial > MAXLINE)
                        lexerror ("Pasted token is too long");
                      if (function_flag)
                        {
                          strcpy (partp, "(: ");
                          partp += 3;
                        }
                      strcpy (partp, yytext);
                      partp += r;
                      outptr += 2;
                    }
                }
              else if (partp != partial)
                {
                  outptr--;
                  if (!expand_define ())
                    add_input (yytext);
                  while ((c = *outptr++) == ' ');
                  outptr--;
                  add_input (partial);
                  partp = partial;
                  partial[0] = 0;
                }
              else
                {
                  outptr--;
                  if (!expand_define ())
                    {
                      ident_hash_elem_t *ihe;
                      if ((ihe = lookup_ident (yytext)))
                        {
                          if (ihe->token & IHE_RESWORD) /* reserved word? */
                            {
                              if (function_flag)
                                {
                                  function_flag = 0;
                                  add_input (yytext);
                                  push_function_context ();
                                  return L_FUNCTION_OPEN;
                                }
                              yylval.number = ihe->sem_value;
                              return ihe->token & TOKEN_MASK;
                            }
                          if (function_flag)
                            {
                              int val;

                              function_flag = 0;
                              while ((c = *outptr++) == ' ');
                              outptr--;
                              if (c != ':' && c != ',')
                                return old_func ();
                              if ((val = ihe->dn.local_num) >= 0)
                                {
                                  if (c == ',')
                                    return old_func ();
                                  yylval.number = (val << 8) | FP_L_VAR;
                                }
                              else if ((val = ihe->dn.global_num) >= 0)
                                {
                                  if (c == ',')
                                    return old_func ();
                                  yylval.number = (val << 8) | FP_G_VAR;
                                }
                              else if ((val = ihe->dn.function_num) >= 0)
                                {
                                  yylval.number = (val << 8) | FP_LOCAL;
                                }
                              else if ((val = ihe->dn.simul_num) >= 0)
                                {
                                  yylval.number = (val << 8) | FP_SIMUL;
                                }
                              else if ((val = ihe->dn.efun_num) >= 0)
                                {
                                  yylval.number = (val << 8) | FP_EFUN;
                                }
                              else
                                return old_func ();
                              return L_NEW_FUNCTION_OPEN;
                            }
                          yylval.ihe = ihe;
                          return L_DEFINED_NAME;
                        }
                      yylval.string = scratch_copy (yytext);
                      return L_IDENTIFIER;
                    }
                  if (function_flag)
                    {
                      function_flag = 0;
                      add_input ("(:");
                    }
                }
              break;
            }
          goto badlex;
        }
    }
badlex:
  {
    return ' ';
  }
}

extern YYSTYPE yylval;

/**
 *  @brief Clean up after finishing a file.
 *  All open include files are closed, and all #if states are cleared.
 *  Also frees any #defines that were added for this file.
 *  Line buffering is also cleared.
 */
void end_new_file () {

  while (inctop)
    {
      incstate_t *p;

      p = inctop;
      close (yyin_desc);
      opt_trace (TT_COMPILE|3, "closed fd = %d (%s)\n", yyin_desc, current_file);
      free_string (current_file);
      current_file = p->file;
      yyin_desc = p->yyin_desc;
      inctop = p->next;
      FREE ((char *) p);
    }
  inctop = 0;
  while (iftop)
    {
      ifstate_t *p;

      p = iftop;
      iftop = p->next;
      FREE ((char *) p);
    }
  if (defines_need_freed)
    {
      free_defines (0);
      defines_need_freed = 0;
    }
  if (cur_lbuf != &head_lbuf)
    {
      linked_buf_t *prev_lbuf;

      while (cur_lbuf != &head_lbuf)
        {
          prev_lbuf = cur_lbuf->prev;
          FREE ((char *) cur_lbuf);
          cur_lbuf = prev_lbuf;
        }
    }
}

static void add_quoted_predefine (char *def, char *val) {
  char save_buf[1024];

  strcpy (save_buf, "\"");
  strcat (save_buf, val);
  strcat (save_buf, "\"");
  add_predefine (def, -1, save_buf);
}

void add_predefines () {
  int i;
  lpc_predef_t *tmpf;

  add_quoted_predefine ("__DRIVER__", PACKAGE);
  add_quoted_predefine ("__VERSION__", VERSION);
  add_predefine ("__LPC__", -1, "1");

  for (i = 0; i < 2 * NUM_OPTION_DEFS; i += 2)
    add_predefine (option_defs[i], -1, option_defs[i + 1]);

  for (tmpf = lpc_predefs; tmpf; tmpf = tmpf->next)
    {
      char namebuf[NSIZE];
      char mtext[MLEN];

      *mtext = '\0';
      sscanf (tmpf->expression, "%[^=]=%[ -~=]", namebuf, mtext);
      if (strlen (namebuf) >= NSIZE)
        fatal ("NSIZE exceeded");
      if (strlen (mtext) >= MLEN)
        fatal ("MLEN exceeded");
      add_predefine (namebuf, -1, mtext);
    }
}

/**
 * @brief Start lexing a new file.
 * 
 * Upon return, the lexer is ready to read from the new file.
 * yylex() can be called to get tokens from the new file.
 * Preprocessor directives such as #include and #define will work
 * as expected.
 * 
 * @param fd The file descriptor of the new file.
 * @param pre_text Optional text to insert before the file contents.
 */
void start_new_file (int fd, const char* pre_text) {
  if (defines_need_freed)
    {
      free_defines (0);
    }
  defines_need_freed = 1;
  if (current_file)
    {
      char *dir;
      char *tmp;
      size_t ln;

      ln = strlen (current_file);
      dir = (char *) DMALLOC (ln + 4, TAG_COMPILER, "start_new_file");
      dir[0] = '"';
      dir[1] = '/';
      memcpy (dir + 2, current_file, ln);
      dir[ln + 2] = '"';
      dir[ln + 3] = 0;
      add_define ("__FILE__", -1, dir);
      tmp = strrchr (dir, '/');
      tmp[1] = '"';
      tmp[2] = 0;
      add_define ("__DIR__", -1, dir);
      FREE (dir);
    }
  yyin_desc = fd; /* lexer input file descriptor */
  lex_fatal = 0;
  last_function_context = -1;
  current_function_context = 0;
  cur_lbuf = &head_lbuf;
  cur_lbuf->outptr = cur_lbuf->buf_end = outptr = cur_lbuf->buf + (DEFMAX >> 1);

  pragmas = DEFAULT_PRAGMAS;
  nexpands = 0;
  incnum = 0;
  current_line = 1;
  current_line_base = 0;
  current_line_saved = 0;

  if (pre_text)
    {
      /* [NEOLITH-EXTENSION] insert any text before the actual file contents */
      add_input (pre_text);
    }
  
  /*
   * Ensure a newline at end of input to allow processor directives at first line.
   * This must be done after any pre_text is added. (i.e. the newline is added before pre_text).
   */
  *(last_nl = outptr - 1) = '\n';

  if (CONFIG_STR (__GLOBAL_INCLUDE_FILE__))
    {
      char gi_file[PATH_MAX];

      /* need a writable copy */
      strncpy (gi_file, CONFIG_STR (__GLOBAL_INCLUDE_FILE__), PATH_MAX - 1);
      gi_file[PATH_MAX - 1] = '\0';
      handle_include (gi_file, 1);
    }
  else
    refill_buffer ();
}

#define add_instr_name(w, x, y, z) int_add_instr_name(w, y, z)

static void int_add_instr_name (char *name, int n, short t) {
  if (n < 0 || n >= MAX_INSTRS)
    {
      fatal ("int_add_instr_name: instruction number %d out of range", n);
    }
  instrs[n].name = name;
  instrs[n].ret_type = t;
}

/**
 * @brief Initialize the number of arguments for LPC eoperators.
 * 
 * This was called init_num_args() in MudOS before.
 * FluffOS renamed it to better reflect its purpose and we agreed.
 */
void init_instrs () {
  int i, n;

  for (i = 0; i < BASE; i++)
    {
      instrs[i].ret_type = -1;
    }
  for (i = 0; i < (int)NELEM (predefs); i++) /* efuns */
    {
      n = predefs[i].token; /* used as instruction number */
      if (n & F_ALIAS_FLAG)
        {
          predefs[i].token ^= F_ALIAS_FLAG;
        }
      else
        {
          instrs[n].min_arg = predefs[i].min_args;
          instrs[n].max_arg = predefs[i].max_args;
          instrs[n].name = predefs[i].word;
          instrs[n].type[0] = predefs[i].arg_type1;
          instrs[n].type[1] = predefs[i].arg_type2;
          instrs[n].type[2] = predefs[i].arg_type3;
          instrs[n].type[3] = predefs[i].arg_type4;
          instrs[n].Default = predefs[i].Default;
          instrs[n].ret_type = predefs[i].ret_type;
          instrs[n].arg_index = predefs[i].arg_index;
        }
    }
  /*
   * eoperators have a return type now.  T_* is used instead of TYPE_*
   * since operators can return multiple types.
   */
  add_instr_name ("<", "c_lt();\n", F_LT, T_NUMBER);
  add_instr_name (">", "c_gt();\n", F_GT, T_NUMBER);
  add_instr_name ("<=", "c_le();\n", F_LE, T_NUMBER);
  add_instr_name (">=", "c_ge();\n", F_GE, T_NUMBER);
  add_instr_name ("==", "f_eq();\n", F_EQ, T_NUMBER);
  add_instr_name ("+=", "c_add_eq(0);\n", F_ADD_EQ, T_ANY);
  add_instr_name ("(void)+=", "c_add_eq(1);\n", F_VOID_ADD_EQ, T_NUMBER);
  add_instr_name ("!", "c_not();\n", F_NOT, T_NUMBER);
  add_instr_name ("&", "f_and();\n", F_AND, T_ARRAY | T_NUMBER);
  add_instr_name ("&=", "f_and_eq();\n", F_AND_EQ, T_NUMBER);
  add_instr_name ("index", "c_index();\n", F_INDEX, T_ANY);
  add_instr_name ("member", "c_member(%i);\n", F_MEMBER, T_ANY);
  add_instr_name ("new_empty_class", "c_new_class(%i, 0);\n", F_NEW_EMPTY_CLASS, T_ANY);
  add_instr_name ("new_class", "c_new_class(%i, 1);\n", F_NEW_CLASS, T_ANY);
  add_instr_name ("rindex", "c_rindex();\n", F_RINDEX, T_ANY);
  add_instr_name ("loop_cond_local", "C_LOOP_COND_LV(%i, %i); if (lpc_int)\n", F_LOOP_COND_LOCAL, -1);
  add_instr_name ("loop_cond_number", "C_LOOP_COND_NUM(%i, %i); if (lpc_int)\n", F_LOOP_COND_NUMBER, -1);
  add_instr_name ("loop_incr", "C_LOOP_INCR(%i);\n", F_LOOP_INCR, -1);
  add_instr_name ("foreach", 0, F_FOREACH, -1);
  add_instr_name ("exit_foreach", "c_exit_foreach();\n", F_EXIT_FOREACH, -1);
  add_instr_name ("expand_varargs", 0, F_EXPAND_VARARGS, -1);
  add_instr_name ("next_foreach", "c_next_foreach();\n", F_NEXT_FOREACH, -1);
  add_instr_name ("member_lvalue", "c_member_lvalue(%i);\n", F_MEMBER_LVALUE, T_LVALUE);
  add_instr_name ("index_lvalue", "push_indexed_lvalue(0);\n", F_INDEX_LVALUE, T_LVALUE | T_LVALUE_BYTE);
  add_instr_name ("rindex_lvalue", "push_indexed_lvalue(1);\n", F_RINDEX_LVALUE, T_LVALUE | T_LVALUE_BYTE);
  add_instr_name ("nn_range_lvalue", "push_lvalue_range(0x00);\n", F_NN_RANGE_LVALUE, T_LVALUE_RANGE);
  add_instr_name ("nr_range_lvalue", "push_lvalue_range(0x01);\n", F_NR_RANGE_LVALUE, T_LVALUE_RANGE);
  add_instr_name ("rr_range_lvalue", "push_lvalue_range(0x11);\n", F_RR_RANGE_LVALUE, T_LVALUE_RANGE);
  add_instr_name ("rn_range_lvalue", "push_lvalue_range(0x10);\n", F_RN_RANGE_LVALUE, T_LVALUE_RANGE);
  add_instr_name ("nn_range", "f_range(0x00);\n", F_NN_RANGE, T_BUFFER | T_ARRAY | T_STRING);
  add_instr_name ("rr_range", "f_range(0x11);\n", F_RR_RANGE, T_BUFFER | T_ARRAY | T_STRING);
  add_instr_name ("nr_range", "f_range(0x01);\n", F_NR_RANGE, T_BUFFER | T_ARRAY | T_STRING);
  add_instr_name ("rn_range", "f_range(0x10);\n", F_RN_RANGE, T_BUFFER | T_ARRAY | T_STRING);
  add_instr_name ("re_range", "f_extract_range(1);\n", F_RE_RANGE, T_BUFFER | T_ARRAY | T_STRING);
  add_instr_name ("ne_range", "f_extract_range(0);\n", F_NE_RANGE, T_BUFFER | T_ARRAY | T_STRING);
  add_instr_name ("global", "C_GLOBAL(%i);\n", F_GLOBAL, T_ANY);
  add_instr_name ("local", "C_LOCAL(%i);\n", F_LOCAL, T_ANY);
  add_instr_name ("transfer_local", "c_transfer_local(%i);\n", F_TRANSFER_LOCAL, T_ANY);
  add_instr_name ("number", 0, F_NUMBER, T_NUMBER);
  add_instr_name ("long", 0, F_LONG, T_NUMBER);
  add_instr_name ("real", 0, F_REAL, T_REAL);
  add_instr_name ("local_lvalue", "C_LVALUE(fp + %i);\n", F_LOCAL_LVALUE, T_LVALUE);
  add_instr_name ("while_dec", "C_WHILE_DEC(%i); if (lpc_int)\n", F_WHILE_DEC, -1);
  add_instr_name ("const1", "push_number(1);\n", F_CONST1, T_NUMBER);
  add_instr_name ("subtract", "c_subtract();\n", F_SUBTRACT, T_NUMBER | T_REAL | T_ARRAY);
  add_instr_name ("(void)assign", "c_void_assign();\n", F_VOID_ASSIGN, T_NUMBER);
  add_instr_name ("(void)assign_local", "c_void_assign_local(fp + %i);\n", F_VOID_ASSIGN_LOCAL, T_NUMBER);
  add_instr_name ("assign", "c_assign();\n", F_ASSIGN, T_ANY);
  add_instr_name ("branch", 0, F_BRANCH, -1);
  add_instr_name ("bbranch", 0, F_BBRANCH, -1);
  add_instr_name ("byte", 0, F_BYTE, T_NUMBER);
  add_instr_name ("-byte", 0, F_NBYTE, T_NUMBER);
  add_instr_name ("branch_ne", 0, F_BRANCH_NE, -1);
  add_instr_name ("branch_ge", 0, F_BRANCH_GE, -1);
  add_instr_name ("branch_le", 0, F_BRANCH_LE, -1);
  add_instr_name ("branch_eq", 0, F_BRANCH_EQ, -1);
  add_instr_name ("bbranch_lt", 0, F_BBRANCH_LT, -1);
  add_instr_name ("bbranch_when_zero", 0, F_BBRANCH_WHEN_ZERO, -1);
  add_instr_name ("bbranch_when_non_zero", 0, F_BBRANCH_WHEN_NON_ZERO, -1);
  add_instr_name ("branch_when_zero", 0, F_BRANCH_WHEN_ZERO, -1);
  add_instr_name ("branch_when_non_zero", 0, F_BRANCH_WHEN_NON_ZERO, -1);
  add_instr_name ("pop", "pop_stack();\n", F_POP_VALUE, -1);
  add_instr_name ("const0", "push_number(0);\n", F_CONST0, T_NUMBER);
#ifdef F_JUMP_WHEN_ZERO
  add_instr_name ("jump_when_zero", F_JUMP_WHEN_ZERO, -1);
  add_instr_name ("jump_when_non_zero", F_JUMP_WHEN_NON_ZERO, -1);
#endif
#ifdef F_LOR
  add_instr_name ("||", 0, F_LOR, -1);
  add_instr_name ("&&", 0, F_LAND, -1);
#endif
  add_instr_name ("-=", "f_sub_eq();\n", F_SUB_EQ, T_ANY);
#ifdef F_JUMP
  add_instr_name ("jump", F_JUMP, -1);
#endif
  add_instr_name ("return_zero", "c_return_zero();\nreturn;\n", F_RETURN_ZERO, -1);
  add_instr_name ("return", "c_return();\nreturn;\n", F_RETURN, -1);
  add_instr_name ("sscanf", "c_sscanf(%i);\n", F_SSCANF, T_NUMBER);
  add_instr_name ("parse_command", "c_parse_command(%i);\n", F_PARSE_COMMAND, T_NUMBER);
  add_instr_name ("string", 0, F_STRING, T_STRING);
  add_instr_name ("short_string", 0, F_SHORT_STRING, T_STRING);
  add_instr_name ("call", "c_call(%i, %i);\n", F_CALL_FUNCTION_BY_ADDRESS, T_ANY);
  add_instr_name ("call_inherited", "c_call_inherited(%i, %i, %i);\n", F_CALL_INHERITED, T_ANY);
  add_instr_name ("aggregate_assoc", "C_AGGREGATE_ASSOC(%i);\n", F_AGGREGATE_ASSOC, T_MAPPING);
  add_instr_name ("aggregate", "C_AGGREGATE(%i);\n", F_AGGREGATE, T_ARRAY);
  add_instr_name ("(::)", 0, F_FUNCTION_CONSTRUCTOR, T_FUNCTION);
  /* sorry about this one */
  add_instr_name ("simul_efun", "call_simul_efun(%i, (lpc_int = %i + num_varargs, num_varargs = 0, lpc_int));\n", F_SIMUL_EFUN, T_ANY);
  add_instr_name ("global_lvalue", "C_LVALUE(&current_object->variables[variable_index_offset + %i]);\n", F_GLOBAL_LVALUE, T_LVALUE);
  add_instr_name ("|", "f_or();\n", F_OR, T_NUMBER);
  add_instr_name ("<<", "f_lsh();\n", F_LSH, T_NUMBER);
  add_instr_name (">>", "f_rsh();\n", F_RSH, T_NUMBER);
  add_instr_name (">>=", "f_rsh_eq();\n", F_RSH_EQ, T_NUMBER);
  add_instr_name ("<<=", "f_lsh_eq();\n", F_LSH_EQ, T_NUMBER);
  add_instr_name ("^", "f_xor();\n", F_XOR, T_NUMBER);
  add_instr_name ("^=", "f_xor_eq();\n", F_XOR_EQ, T_NUMBER);
  add_instr_name ("|=", "f_or_eq();\n", F_OR_EQ, T_NUMBER);
  add_instr_name ("+", "c_add();\n", F_ADD, T_ANY);
  add_instr_name ("!=", "f_ne();\n", F_NE, T_NUMBER);
  add_instr_name ("catch", 0, F_CATCH, T_ANY);
  add_instr_name ("end_catch", 0, F_END_CATCH, -1);
  add_instr_name ("-", "c_negate();\n", F_NEGATE, T_NUMBER | T_REAL);
  add_instr_name ("~", "c_compl();\n", F_COMPL, T_NUMBER);
  add_instr_name ("++x", "c_pre_inc();\n", F_PRE_INC, T_NUMBER | T_REAL);
  add_instr_name ("--x", "c_pre_dec();\n", F_PRE_DEC, T_NUMBER | T_REAL);
  add_instr_name ("*", "c_multiply();\n", F_MULTIPLY, T_REAL | T_NUMBER | T_MAPPING);
  add_instr_name ("*=", "f_mult_eq();\n", F_MULT_EQ, T_REAL | T_NUMBER | T_MAPPING);
  add_instr_name ("/", "c_divide();\n", F_DIVIDE, T_REAL | T_NUMBER);
  add_instr_name ("/=", "f_div_eq();\n", F_DIV_EQ, T_NUMBER | T_REAL);
  add_instr_name ("%", "c_mod();\n", F_MOD, T_NUMBER);
  add_instr_name ("%=", "f_mod_eq();\n", F_MOD_EQ, T_NUMBER);
  add_instr_name ("inc(x)", "c_inc();\n", F_INC, -1);
  add_instr_name ("dec(x)", "c_dec();\n", F_DEC, -1);
  add_instr_name ("x++", "c_post_inc();\n", F_POST_INC, T_NUMBER | T_REAL);
  add_instr_name ("x--", "c_post_dec();\n", F_POST_DEC, T_NUMBER | T_REAL);
  add_instr_name ("switch", 0, F_SWITCH, -1);
  add_instr_name ("time_expression", 0, F_TIME_EXPRESSION, -1);
  add_instr_name ("end_time_expression", 0, F_END_TIME_EXPRESSION, T_NUMBER);
}

void deinit_instrs (void) {
  memset (instrs, 0, sizeof(instrs));
}

/**
 * @brief Get the name of an LPC opcode (stack machine instructions).
 * 
 * This function was called get_f_name() in MudOS before.
 * @param n The opcode number.
 * @return The name of the opcode, or "<OTHER n>" if it has no name.
 */
const char *query_opcode_name (int n) {
  if (n >= 0 && n < MAX_INSTRS && instrs[n].name)
    return instrs[n].name;
  else
    {
      static char buf[30];

      sprintf (buf, "<OTHER %d>", n);
      return buf;
    }
}

#define get_next_char(c) if ((c = *outptr++) == '\n' && outptr == last_nl + 1) refill_buffer()

#define GETALPHA(p, q, m) \
    while(isalunum(*p)) {\
        *q = *p++;\
        if (q < (m))\
            q++;\
        else {\
            lexerror("Name too long");\
            return;\
        }\
    }\
    *q++ = 0

/* kludge to allow token pasting */
#define GETDEFINE(p, q, m) \
    while (isalunum(*p) || (*p == '#')) {\
       *q = *p++; \
       if (q < (m)) \
           q++; \
       else { \
           lexerror("Name too long"); \
           return; \
       } \
    } \
    *q++ = 0

static int cmygetc () {
  int c;

  for (;;)
    {
      get_next_char (c);
      if (c == '/')
        {
          switch (*outptr++)
            {
            case '*':
              skip_comment ();
              break;
            case '/':
              skip_line ();
              break;
            default:
              outptr--;
              return c;
            }
        }
      else
        {
          return c;
        }
    }
}

static void refill () {
  char *p;
  int c;

  p = yytext;
  do
    {
      c = *outptr++;
      if (p < yytext + MAXLINE - 5)
        *p++ = (char)c;
      else
        {
          lexerror ("Line too long");
          break;
        }
    }
  while (c != '\n' && c != LEX_EOF);
  if ((c == '\n') && (outptr == last_nl + 1))
    refill_buffer ();
  p[-1] = ' ';
  *p = 0;
  nexpands = 0;
  current_line++;
}

static void handle_define (char *yyt) {
  char namebuf[NSIZE];
  char args[NARGS][NSIZE];
  char mtext[MLEN];
  char *p, *q;

  p = yyt;
  strcat (p, " ");
  q = namebuf;
  GETALPHA (p, q, namebuf + NSIZE - 1);
  if (*p == '(')
    {				/* if "function macro" */
      int arg;
      int inid;
      char *ids = (char *) NULL;

      p++;			/* skip '(' */
      SKIPWHITE;
      if (*p == ')')
        {
          arg = 0;
        }
      else
        {
          for (arg = 0; arg < NARGS;)
            {
              q = args[arg];
              GETDEFINE (p, q, args[arg] + NSIZE - 1);
              arg++;
              SKIPWHITE;
              if (*p == ')')
                break;
              if (*p++ != ',')
                {
                  yyerror ("Missing ',' in #define parameter list");
                  return;
                }
              SKIPWHITE;
            }
          if (arg == NARGS)
            {
              lexerror ("Too many macro arguments");
              return;
            }
        }
      p++;			/* skip ')' */
      q = mtext;
      *q++ = ' ';
      for (inid = 0; *p;)
        {
          if (isalunum (*p))
            {
              if (!inid)
                {
                  inid++;
                  ids = p;
                }
            }
          else
            {
              if (inid)
                {
                  size_t l, idlen = p - ids;
                  int n;

                  for (n = 0; n < arg; n++)
                    {
                      l = strlen (args[n]);
                      if (l == idlen && strncmp (args[n], ids, l) == 0)
                        {
                          q -= idlen;
                          *q++ = MARKS;
                          *q++ = (char)(n + MARKS + 1);
                          break;
                        }
                    }
                  inid = 0;
                }
            }
          *q = *p;
          if (*p++ == MARKS)
            *++q = MARKS;
          if (q < mtext + MLEN - 2)
            q++;
          else
            {
              lexerror ("Macro text too long");
              return;
            }
          if (!*p && p[-2] == '\\')
            {
              q -= 2;
              refill ();
              p = yytext;
            }
        }
      *--q = 0;
      add_define (namebuf, arg, mtext);
    }
  else if (is_wspace (*p) || (*p == '\\'))
    {
      for (q = mtext; *p;)
        {
          *q = *p++;
          if (q < mtext + MLEN - 2)
            q++;
          else
            {
              lexerror ("Macro text too long");
              return;
            }
          if (!*p && p[-2] == '\\')
            {
              q -= 2;
              refill ();
              p = yytext;
            }
        }
      *--q = 0;
      add_define (namebuf, -1, mtext);
    }
  else
    {
      lexerror ("Illegal macro symbol");
    }
  return;
}

/* IDEA: linked buffers, to allow "unlimited" buffer expansion */
/**
 * @brief Add input to the macro expansion buffer.
 *
 * cur_lbuf is a linked list of buffers used for macro expansion.
 * It is extended as needed to allow for large macro expansions.
 * @param p The string to add.
 */
static void add_input (const char *p) {
  size_t len = strlen (p);

  if (len >= DEFMAX - 10)
    {
      lexerror ("Macro expansion buffer overflow");
      return;
    }

  if (outptr < len + 5 + cur_lbuf->buf)
    {
      /* Not enough space, so let's move it up another linked_buf */
      linked_buf_t *new_lbuf;
      char *q, *new_outp, *buf;
      size_t size;

      q = outptr;

      while (*q != '\n' && *q != LEX_EOF)
        q++;
      /* Incorporate EOF later */
      if (*q != '\n' || ((q - outptr) + len) >= DEFMAX - 11)
        {
          lexerror ("Macro expansion buffer overflow");
          return;
        }
      size = (q - outptr) + len + 1; /* remaining of current line of source code */
      cur_lbuf->outptr = q + 1;
      cur_lbuf->last_nl = last_nl;

      new_lbuf = ALLOCATE (linked_buf_t, TAG_COMPILER, "add_input");
      new_lbuf->term_type = TERM_ADD_INPUT;
      new_lbuf->prev = cur_lbuf;
      buf = new_lbuf->buf;
      cur_lbuf = new_lbuf;
      last_nl = (new_lbuf->buf_end = buf + DEFMAX - 2) - 1;
      new_outp = new_lbuf->outptr = buf + DEFMAX - 2 - size;
      memcpy (new_outp, p, len);
      memcpy (new_outp + len, outptr, (q - outptr) + 1);
      outptr = new_outp;
      *(last_nl + 1) = 0;
      return;
    }

  outptr -= len;
  memcpy (outptr, p, len);
}

static void add_predefine (char *name, int nargs, char *exps) {
  defn_t *p;
  int h;

  if ((p = lookup_define (name)))
    {
      if (nargs != p->nargs || strcmp (exps, p->exps))
        {
          char buf[200 + NSIZE];
          sprintf (buf, "redefinition of #define %s\n", name);
          yywarn (buf);
        }
      p->exps = (char *) DREALLOC (p->exps, strlen (exps) + 1, TAG_PREDEFINES, "add_define: redef");
      strcpy (p->exps, exps);
      p->nargs = nargs;
    }
  else
    {
      p = ALLOCATE (defn_t, TAG_PREDEFINES, "add_define: def");
      p->name = (char *) DXALLOC (strlen (name) + 1, TAG_PREDEFINES, "add_define: def name");
      strcpy (p->name, name);
      p->exps = (char *) DXALLOC (strlen (exps) + 1, TAG_PREDEFINES, "add_define: def exps");
      strcpy (p->exps, exps);
      p->flags = DEF_IS_PREDEF;
      p->nargs = nargs;
      h = defhash (name);
      p->next = defns[h];
      defns[h] = p;
    }
}

/**
 * @brief Free all defines.
 * @param include_predefs If true, also free predefined macros.
 */
void free_defines (int include_predefs) {
  defn_t *p, *q;
  int i;

  for (i = 0; i < DEFHASH; i++)
    {
      for (p = defns[i]; p; p = q)
        {
          /* predefines are at the end of the list */
          if (!include_predefs && (p->flags & DEF_IS_PREDEF))
            break;
          // opt_trace (TT_COMPILE|2, "predef: %s\n", p->name);
          q = p->next;
          FREE (p->name);
          FREE (p->exps);
          FREE ((char *) p);
        }
      defns[i] = p;
      /* in case they undefined a predef */
      while (p)
        {
          p->flags &= ~DEF_IS_UNDEFINED; /* clear undefined flag */
          p = p->next;
        }
    }
  nexpands = 0;
}

#define SKIPW \
        do {\
            c = cmygetc();\
        } while(is_wspace(c));


/* Check if yytext is a macro and expand if it is. */
static int expand_define () {
  defn_t *p;
  char expbuf[DEFMAX];
  char *args[NARGS];
  char buf[DEFMAX];
  char *q, *e, *b;

  if (nexpands++ > EXPANDMAX)
    {
      lexerror ("Too many macro expansions");
      return 0;
    }
  p = lookup_define (yytext);
  if (!p)
    {
      return 0;
    }
  if (p->nargs == -1)
    {
      add_input (p->exps);
    }
  else
    {
      int c, parcnt = 0, dquote = 0, squote = 0;
      int n;

      SKIPW;
      if (c != '(')
        {
          yyerror ("Missing '(' in macro call");
          if (c == '\n' && outptr == last_nl + 1)
            refill_buffer ();
          return 0;
        }
      SKIPW;
      if (c == ')')
        n = 0;
      else
        {
          q = expbuf;
          args[0] = q;
          for (n = 0; n < NARGS;)
            {
              switch (c)
                {
                case '"':
                  if (!squote)
                    dquote ^= 1;
                  break;
                case '\'':
                  if (!dquote)
                    squote ^= 1;
                  break;
                case '(':
                  if (!squote && !dquote)
                    parcnt++;
                  break;
                case ')':
                  if (!squote && !dquote)
                    parcnt--;
                  break;
                case '#':
                  if (!squote && !dquote)
                    {
                      *q++ = (char)c;
                      if (*outptr++ != '#')
                        {
                          lexerror ("'#' expected");
                          return 0;
                        }
                    }
                  break;
                case '\\':
                  if (squote || dquote)
                    {
                      *q++ = (char)c;
                      get_next_char (c);
                    }
                  break;
                case '\n':
                  if (outptr == last_nl + 1)
                    refill_buffer ();
                  if (squote || dquote)
                    {
                      lexerror ("Newline in string");
                      return 0;
                    }
                  /* Change this to a space so we don't count it a variable
                     number of times based on how many times it is used
                     in the expansion */
                  current_line++;
                  total_lines++;
                  c = ' ';
                  break;
                }
              if (c == ',' && !parcnt && !dquote && !squote)
                {
                  *q++ = 0;
                  args[++n] = q;
                }
              else if (parcnt < 0)
                {
                  *q++ = 0;
                  n++;
                  break;
                }
              else
                {
                  if (c == LEX_EOF)
                    {
                      lexerror ("Unexpected end of file");
                      return 0;
                    }
                  if (q >= expbuf + DEFMAX - 5)
                    {
                      lexerror ("Macro argument overflow");
                      return 0;
                    }
                  else
                    {
                      *q++ = (char)c;
                    }
                }
              if (!squote && !dquote)
                c = cmygetc ();
              else
                {
                  get_next_char (c);
                }
            }
          if (n == NARGS)
            {
              lexerror ("Maximum macro argument count exceeded");
              return 0;
            }
        }
      if (n != p->nargs)
        {
          yyerror ("Wrong number of macro arguments");
          return 0;
        }
      /* Do expansion */
      b = buf;
      e = p->exps;
      while (*e)
        {
          if (*e == '#' && *(e + 1) == '#')
            e += 2;
          if (*e == MARKS)
            {
              if (*++e == MARKS)
                *b++ = *e++;
              else
                {
                  for (q = args[*e++ - MARKS - 1]; *q;)
                    {
                      *b++ = *q++;
                      if (b >= buf + DEFMAX)
                        {
                          lexerror ("Macro expansion overflow");
                          return 0;
                        }
                    }
                }
            }
          else
            {
              *b++ = *e++;
              if (b >= buf + DEFMAX)
                {
                  lexerror ("Macro expansion overflow");
                  return 0;
                }
            }
        }
      *b++ = 0;
      add_input (buf);
    }
  return 1;
}

/* Stuff to evaluate expression.  I havn't really checked it. /LA
** Written by "J\"orn Rennecke" <amylaar@cs.tu-berlin.de>
*/
#define SKPW 	do{c=*outptr++;}while(is_wspace(c)); outptr--

static int exgetc () {
  register char c, *yyp;

  c = *outptr++;
  while (isalpha (c) || c == '_')
    {
      yyp = yytext;
      do
        {
          SAVEC;
          c = *outptr++;
        }
      while (isalunum (c));
      outptr--;
      *yyp = '\0';
      if (strcmp (yytext, "defined") == 0)
        {
          /* handle the defined "function" in #if */
          do
            c = *outptr++;
          while (is_wspace (c));
          if (c != '(')
            {
              yyerror ("Missing ( in defined");
              continue;
            }
          do
            c = *outptr++;
          while (is_wspace (c));
          yyp = yytext;
          while (isalunum (c))
            {
              SAVEC;
              c = *outptr++;
            }
          *yyp = '\0';
          while (is_wspace (c))
            c = *outptr++;
          if (c != ')')
            {
              yyerror ("Missing ) in defined");
              continue;
            }
          SKPW;
          if (lookup_define (yytext))
            add_input (" 1 ");
          else
            add_input (" 0 ");
        }
      else
        {
          if (!expand_define ())
            add_input (" 0 ");
        }
      c = *outptr++;
    }
  return c;
}

/**
 * @brief Set the include search path.
 * @param list A colon-separated list of directories.
 */
void set_inc_list (const char *list) {
  int i, size;
  char *list_copy, *p;

  if (!list || !*list)
    return; /* it's ok to not having inc_list */

  if (mbstowcs (NULL, list, 0) != strlen(list))
    {
      debug_fatal ("non-ANSI characters are not allowed in include search path.");
      exit (EXIT_FAILURE);
    }
  debug_message ("{}\tusing LPC header search path: %s", list);

  size = 1;
  p = list_copy = xstrdup(list); /* make a copy we can modify */
  while (1)
    {
      p = strchr (p, ':');
      if (!p)
        break;
      size++;
      p++;
    }
  inc_list = CALLOCATE (size, char *, TAG_INC_LIST, "set_inc_list");
  inc_list_size = size;
  for (i = size - 1; i >= 0; i--)
    {
      p = strrchr (list, ':'); /* get one path from the end of list */
      if (p)
        {
          *p = '\0';
          p++;
        }
      else
        {
          if (i)
            {
              while (i>0)
                inc_list[i--] = 0;
            }
          p = list_copy;
        }
      if (*p == '/')
        p++;

      if (!legal_path (p))
        {
          debug_warn ("unsafe directory removed from include search path: %s", p);
          inc_list[i] = 0;
          continue;
        }
      inc_list[i] = make_shared_string (p);
    }
  free (list_copy);
}

/**
 * @brief Reset the include search path.
 */
void reset_inc_list (void) {
  int i;
  if (inc_list)
    {
      for (i = 0; i < inc_list_size; i++)
        {
          if (inc_list[i])
            free_string (inc_list[i]);
        }
      FREE (inc_list);
      inc_list = NULL;
      inc_list_size = 0;
    }
}

const char* main_file_name () {
  incstate_t *is;

  if (inctop == 0)
    return current_file;
  is = inctop;
  while (is->next)
    is = is->next;
  return is->file;
}

size_t init_keywords (void) {
  size_t i;

  for (i = 0; i < sizeof(reswords) / sizeof(reswords[0]); i++)
    {
      keyword_t *entry = &reswords[i];
      add_keyword (entry->word, entry);
    }
  return i;
}

void init_predefines (void) {
  /* add the efuns */
  size_t i;
  ident_hash_elem_t *ihe;
  for (i = 0; i < sizeof(predefs) / sizeof(predefs[0]); i++)
    {
      ihe = find_or_add_perm_ident (predefs[i].word, IHE_EFUN);
      ihe->sem_value++;
      ihe->dn.efun_num = (short)i;
    }
}
