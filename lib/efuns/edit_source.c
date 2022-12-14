/*  $Id: edit_source.c,v 1.2 2003/01/04 04:10:31 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef	STDC_HEADERS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <locale.h>
#endif /* STDC_HEADERS */

#ifdef	HAVE_ARGP_H
#include <argp.h>
#endif /* HAVE_ARGP_H */

#define CONFIGURE_VERSION	5

#define EDIT_SOURCE
#define NO_MALLOC
#define NO_SOCKETS
#define NO_OPCODES
#include "src/lex.h"
#include "src/preprocess.h"
#include "edit_source.h"
#include "hash.h"
#include "lib/wrapper.h"

#define TO_DEV_NULL ">/dev/null 2>&1"

char *outp;
static int buffered = 0;
static int nexpands = 0;

FILE *yyin = 0, *yyout = 0;

#define HEADER_OPTION		".option.h"

#define HEADER_VECTOR		".vector.h"
#define HEADER_OPCODE		".opcode.h"
#define HEADER_PROTOTYPE	".prototype.h"
#define HEADER_DEFINITION	".definition.h"

#define PRAGMA_NOTE_CASE_START 1

int num_packages = 0;
char *packages[100];
char ppchar;

char *current_file = 0;
int current_line;

int grammar_mode = 0;		/* which section we're in for .y files */
int in_c_case, cquote, pragmas, block_nest;

char yytext[MAXLINE];
static char defbuf[DEFMAX];

typedef struct incstate_t
{
  struct incstate_t *next;
  FILE *yyin;
  int line;
  char *file;
}
incstate;

static incstate *inctop = 0;

#define CHAR_QUOTE 1
#define STRING_QUOTE 2

static void add_define (char *, int, char *);
int yyparse (void);
static void handle_options (char *fname);
static void handle_build_efuns (const char *efun_spec);

/* implementations */

int
main (int argc, char *argv[])
{
#if defined(HAVE_GETTEXT) && defined(ENABLE_NLS)
  /* ?}?ҥ??a?y?t */
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
#endif /* ENABLE_NLS */

  if (argc < 3)
    {
      fprintf (stderr, "usage: %s OPTION LPC-SPEC\n", argv[0]);
      exit (EXIT_FAILURE);
    }

  fprintf (stderr, "----------------------------------------------\n");
  handle_options (*++argv);
  handle_build_efuns (*++argv);
  fprintf (stderr, "----------------------------------------------\n");

  return 0;
}

void
yyerror (char *str)
{
  fprintf (stderr, "%s:%d: %s\n", current_file, current_line, str);
  exit (1);
}

void
yywarn (char *str)
{
  /* ignore errors :)  local_options generates redefinition warnings,
     which we don't want to see */
}

void
yyerrorp (char *str)
{
  char buff[200];
  sprintf (buff, str, ppchar);
  fprintf (stderr, "%s:%d: %s\n", current_file, current_line, buff);
  exit (1);
}

static void
add_input (char *p)
{
  int l = strlen (p);

  if (outp - l < defbuf)
    yyerror ("Macro expansion buffer overflow.\n");
  strncpy (outp - l, p, l);
  outp -= l;
}

#define SKIPW(foo) while (isspace(*foo)) foo++;

static char *
skip_comment (char *tmp, int flag)
{
  int c;

  for (;;)
    {
      while ((c = *++tmp) != '*')
	{
	  if (c == EOF)
	    yyerror ("End of file in a comment");
	  if (c == '\n')
	    {
	      nexpands = 0;
	      current_line++;
	      if (!fgets (yytext, MAXLINE - 1, yyin))
		yyerror ("End of file in a comment");
	      if (flag && yyout)
		fputs (yytext, yyout);
	      tmp = yytext - 1;
	    }
	}
      do
	{
	  if ((c = *++tmp) == '/')
	    return tmp + 1;
	  if (c == '\n')
	    {
	      nexpands = 0;
	      current_line++;
	      if (!fgets (yytext, MAXLINE - 1, yyin))
		yyerror ("End of file in a comment");
	      if (flag && yyout)
		fputs (yytext, yyout);
	      tmp = yytext - 1;
	    }
	}
      while (c == '*');
    }
}

static void
refill ()
{
  register char *p, *yyp;
  int c;

  if (fgets (p = yyp = defbuf + (DEFMAX >> 1), MAXLINE - 1, yyin))
    {
      while (((c = *yyp++) != '\n') && (c != EOF))
	{
	  if (c == '/')
	    {
	      if ((c = *yyp) == '*')
		{
		  yyp = skip_comment (yyp, 0);
		  continue;
		}
	      else if (c == '/')
		break;
	    }
	  *p++ = c;
	}
    }
  else
    yyerror ("End of macro definition in \\");
  nexpands = 0;
  current_line++;
  *p = 0;
  return;
}

static void
handle_define ()
{
  char namebuf[NSIZE];
  char args[NARGS][NSIZE];
  char mtext[MLEN];
  char *end;
  register char *tmp = outp, *q;

  q = namebuf;
  end = q + NSIZE - 1;
  while (isalunum (*tmp))
    {
      if (q < end)
	*q++ = *tmp++;
      else
	yyerror ("Name too long.\n");
    }
  if (q == namebuf)
    yyerror ("Macro name missing.\n");
  *q = 0;
  if (*tmp == '(')
    {				/* if "function macro" */
      int arg;
      int inid;
      char *ids = (char *) NULL;

      tmp++;			/* skip '(' */
      SKIPW (tmp);
      if (*tmp == ')')
	{
	  arg = 0;
	}
      else
	{
	  for (arg = 0; arg < NARGS;)
	    {
	      end = (q = args[arg]) + NSIZE - 1;
	      while (isalunum (*tmp) || (*tmp == '#'))
		{
		  if (q < end)
		    *q++ = *tmp++;
		  else
		    yyerror ("Name too long.\n");
		}
	      if (q == args[arg])
		{
		  char buff[200];
		  sprintf (buff,
			   "Missing argument %d in #define parameter list",
			   arg + 1);
		  yyerror (buff);
		}
	      arg++;
	      SKIPW (tmp);
	      if (*tmp == ')')
		break;
	      if (*tmp++ != ',')
		{
		  yyerror ("Missing ',' in #define parameter list");
		}
	      SKIPW (tmp);
	    }
	  if (arg == NARGS)
	    yyerror ("Too many macro arguments");
	}
      tmp++;			/* skip ')' */
      end = mtext + MLEN - 2;
      for (inid = 0, q = mtext; *tmp;)
	{
	  if (isalunum (*tmp))
	    {
	      if (!inid)
		{
		  inid++;
		  ids = tmp;
		}
	    }
	  else
	    {
	      if (inid)
		{
		  int idlen = tmp - ids;
		  int n, l;

		  for (n = 0; n < arg; n++)
		    {
		      l = strlen (args[n]);
		      if (l == idlen && strncmp (args[n], ids, l) == 0)
			{
			  q -= idlen;
			  *q++ = MARKS;
			  *q++ = n + MARKS + 1;
			  break;
			}
		    }
		  inid = 0;
		}
	    }
	  if ((*q = *tmp++) == MARKS)
	    *++q = MARKS;
	  if (q < end)
	    q++;
	  else
	    yyerror ("Macro text too long");
	  if (!*tmp && tmp[-2] == '\\')
	    {
	      q -= 2;
	      refill ();
	      tmp = defbuf + (DEFMAX >> 1);
	    }
	}
      *--q = 0;
      add_define (namebuf, arg, mtext);
    }
  else if (isspace (*tmp) || (!*tmp && (*(tmp + 1) = '\0', *tmp = ' ')))
    {
      end = mtext + MLEN - 2;
      for (q = mtext; *tmp;)
	{
	  *q = *tmp++;
	  if (q < end)
	    q++;
	  else
	    yyerror ("Macro text too long");
	  if (!*tmp && tmp[-2] == '\\')
	    {
	      q -= 2;
	      refill ();
	      tmp = defbuf + (DEFMAX >> 1);
	    }
	}
      *q = 0;
      add_define (namebuf, -1, mtext);
    }
  else
    {
      yyerror ("Illegal macro symbol");
    }
  return;
}

#define SKPW while (isspace(*outp)) outp++

static int
cmygetc ()
{
  int c;

  for (;;)
    {
      if ((c = *outp++) == '/')
	{
	  if ((c = *outp) == '*')
	    outp = skip_comment (outp, 0);
	  else if (c == '/')
	    return -1;
	  else
	    return c;
	}
      else
	return c;
    }
}

/* Check if yytext is a macro and expand if it is. */
static int
expand_define ()
{
  defn_t *p;
  char expbuf[DEFMAX];
  char *args[NARGS];
  char buf[DEFMAX];
  char *q, *e, *b;

  if (nexpands++ > EXPANDMAX)
    yyerror ("Too many macro expansions");
  if (!(p = lookup_define (yytext)))
    return 0;
  if (p->nargs == -1)
    {
      add_input (p->exps);
    }
  else
    {
      int c, parcnt = 0, dquote = 0, squote = 0;
      int n;

      SKPW;
      if (*outp++ != '(')
	yyerror ("Missing '(' in macro call");
      SKPW;
      if ((c = *outp++) == ')')
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
		      *q++ = c;
		      if (*outp++ != '#')
			yyerror ("'#' expected");
		    }
		  break;
		case '\\':
		  if (squote || dquote)
		    {
		      *q++ = c;
		      c = *outp++;
		    }
		  break;
		case '\n':
		  if (squote || dquote)
		    yyerror ("Newline in string");
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
		  if (c == EOF)
		    yyerror ("Unexpected end of file");
		  if (q >= expbuf + DEFMAX - 5)
		    {
		      yyerror ("Macro argument overflow");
		    }
		  else
		    {
		      *q++ = c;
		    }
		}
	      if (!squote && !dquote)
		{
		  if ((c = cmygetc ()) < 0)
		    yyerror ("End of macro in // comment");
		}
	      else
		c = *outp++;
	    }
	  if (n == NARGS)
	    {
	      yyerror ("Maximum macro argument count exceeded");
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
			yyerror ("Macro expansion overflow");
		    }
		}
	    }
	  else
	    {
	      *b++ = *e++;
	      if (b >= buf + DEFMAX)
		yyerror ("Macro expansion overflow");
	    }
	}
      *b++ = 0;
      add_input (buf);
    }
  return 1;
}

static int
exgetc ()
{
  register char c, *yyp;

  SKPW;
  while (isalpha (c = *outp) || c == '_')
    {
      yyp = yytext;
      do
	{
	  *yyp++ = c;
	}
      while (isalnum (c = *++outp) || (c == '_'));
      *yyp = '\0';
      if (!strcmp (yytext, "defined"))
	{
	  /* handle the defined "function" in #/%if */
	  SKPW;
	  if (*outp++ != '(')
	    yyerror ("Missing ( after 'defined'");
	  SKPW;
	  yyp = yytext;
	  if (isalpha (c = *outp) || c == '_')
	    {
	      do
		{
		  *yyp++ = c;
		}
	      while (isalnum (c = *++outp) || (c == '_'));
	      *yyp = '\0';
	    }
	  else
	    yyerror ("Incorrect definition macro after defined(\n");
	  SKPW;
	  if (*outp != ')')
	    yyerror ("Missing ) in defined");
	  if (lookup_define (yytext))
	    add_input ("1 ");
	  else
	    add_input ("0 ");
	}
      else
	{
	  if (!expand_define ())
	    add_input ("0 ");
	  else
	    SKPW;
	}
    }
  return c;
}

static int
skip_to (char *token, char *atoken)
{
  char b[20], *p, *end;
  int c;
  int nest;

  for (nest = 0;;)
    {
      if (!fgets (outp = defbuf + (DEFMAX >> 1), MAXLINE - 1, yyin))
	{
	  yyerror ("Unexpected end of file while skipping");
	}
      current_line++;
      if ((c = *outp++) == ppchar)
	{
	  while (isspace (*outp))
	    outp++;
	  end = b + sizeof b - 1;
	  for (p = b; (c = *outp++) != '\n' && !isspace (c) && c != EOF;)
	    {
	      if (p < end)
		*p++ = c;
	    }
	  *p = 0;
	  if (!strcmp (b, "if") || !strcmp (b, "ifdef")
	      || !strcmp (b, "ifndef"))
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
		  *--outp = c;
		  add_input (b);
		  *--outp = ppchar;
		  buffered = 1;
		  return 1;
		}
	      else if (atoken && !strcmp (b, atoken))
		{
		  *--outp = c;
		  add_input (b);
		  *--outp = ppchar;
		  buffered = 1;
		  return 0;
		}
	      else if (!strcmp (b, "elif"))
		{
		  *--outp = c;
		  add_input (b);
		  *--outp = ppchar;
		  buffered = 1;
		  return !atoken;
		}
	    }
	}
    }
}

#include "src/preprocess.c"

static void
open_input_file (const char *filename)
{
  /* open input file */
  if (NULL == (yyin = fopen (filename, "r")))
    {
      perror (filename);
      exit (EXIT_FAILURE);
    }

  /* keep the file name */
  if (current_file)
    free ((char *) current_file);
  current_file = strdup (filename);

  /* reset line number */
  current_line = 0;
}

static void
open_output_file (char *filename)
{
  if (NULL == (yyout = fopen (filename, "w")))
    {
      perror (filename);
      exit (EXIT_FAILURE);
    }
}

static void
close_output_file ()
{
  fclose (yyout);
  yyout = NULL;
}

static char *
protect (char *p)
{
  static char buf[1024];
  char *bufp = buf;

  while (*p)
    {
      if (*p == '\"' || *p == '\\')
	*bufp++ = '\\';
      *bufp++ = *p++;
    }
  *bufp = 0;
  return buf;
}

static void
create_option_defines ()
{
  defn_t *p;
  int count = 0;
  int i;

  fprintf (stderr, _("Writing option definitions to %s ...\n"),
	   HEADER_OPTION);
  open_output_file (HEADER_OPTION);
  fprintf (yyout, "{\n");

  for (i = 0; i < DEFHASH; i++)
    {
      for (p = defns[i]; p; p = p->next)
	if (!(p->flags & DEF_IS_UNDEFINED))
	  {
	    count++;
	    fprintf (yyout, "  \"__%s__\", \"%s\",\n",
		     p->name, protect (p->exps));
	    if (strncmp (p->name, "PACKAGE_", 8) == 0)
	      {
		int len;
		char *tmp, *t;

		len = strlen (p->name + 8);
		t = tmp = (char *) malloc (len + 1);
		strcpy (tmp, p->name + 8);
		while (*t)
		  {
		    if (isupper (*t))
		      *t = tolower (*t);
		    t++;
		  }
		if (num_packages == 100)
		  {
		    fprintf (stderr, "Too many packages.\n");
		    exit (-1);
		  }
		packages[num_packages++] = tmp;
	      }
	  }
    }
  fprintf (yyout, "};\n\n#define NUM_OPTION_DEFS %d\n\n", count);
  close_output_file ();
}

static void
deltrail ()
{
  register char *p;

  p = outp;
  while (*p && !isspace (*p) && *p != '\n')
    {
      p++;
    }
  *p = 0;
}

static void
handle_include (char *name)
{
  char *p;
  static char buf[1024];
  FILE *f;
  incstate *is;

  if (*name != '"')
    {
      defn_t *d;

      if ((d = lookup_define (name)) && d->nargs == -1)
	{
	  char *q;

	  q = d->exps;
	  while (isspace (*q))
	    q++;
	  handle_include (q);
	}
      else
	{
	  yyerrorp ("Missing leading \" in %cinclude");
	}
      return;
    }
  for (p = ++name; *p && *p != '"'; p++);
  if (!*p)
    yyerrorp ("Missing trailing \" in %cinclude");

  *p = 0;
  if ((f = fopen (name, "r")) != NULL)
    {
      is = (incstate *)
	malloc (sizeof (incstate) /*, 61, "handle_include: 1" */ );
      is->yyin = yyin;
      is->line = current_line;
      is->file = current_file;
      is->next = inctop;
      inctop = is;
      current_line = 0;
      current_file =
	(char *) malloc (strlen (name) + 1 /*, 62, "handle_include: 2" */ );
      strcpy (current_file, name);
      yyin = f;
    }
  else
    {
      sprintf (buf, "Cannot %cinclude %s", ppchar, name);
      yyerror (buf);
    }
}

static void
handle_pragma (char *name)
{
  if (!strcmp (name, "auto_note_compiler_case_start"))
    pragmas |= PRAGMA_NOTE_CASE_START;
  else if (!strcmp (name, "no_auto_note_compiler_case_start"))
    pragmas &= ~PRAGMA_NOTE_CASE_START;
  else if (!strncmp (name, "ppchar:", 7) && *(name + 8))
    ppchar = *(name + 8);
  else
    yyerrorp ("Unidentified %cpragma");
}

static void
preprocess ()
{
  register char *yyp, *yyp2;
  int c;
  int cond;

  while (buffered ? (yyp = yyp2 = outp) :
	 fgets (yyp = yyp2 = defbuf + (DEFMAX >> 1), MAXLINE - 1, yyin))
    {
      if (!buffered)
	current_line++;
      else
	buffered = 0;
      while (isspace (*yyp2))
	yyp2++;
      if ((c = *yyp2) == ppchar)
	{
	  int quote = 0;
	  char sp_buf = 0, *oldoutp = NULL;

	  if (c == '%' && yyp2[1] == '%')
	    grammar_mode++;
	  outp = 0;
	  if (yyp != yyp2)
	    yyerrorp ("Misplaced '%c'.\n");
	  while (isspace (*++yyp2));
	  yyp++;
	  for (;;)
	    {
	      if ((c = *yyp2++) == '"')
		quote ^= 1;
	      else
		{
		  if (!quote && c == '/')
		    {
		      if (*yyp2 == '*')
			{
			  yyp2 = skip_comment (yyp2, 0);
			  continue;
			}
		      else if (*yyp2 == '/')
			break;
		    }
		  if (!outp && isspace (c))
		    outp = yyp;
		  if (c == '\n' || c == EOF)
		    break;
		}
	      *yyp++ = c;
	    }

	  if (outp)
	    {
	      if (yyout)
		sp_buf = *(oldoutp = outp);
	      *outp++ = 0;
	      while (isspace (*outp))
		outp++;
	    }
	  else
	    outp = yyp;
	  *yyp = 0;
	  yyp = defbuf + (DEFMAX >> 1) + 1;

	  if (!strcmp ("define", yyp))
	    {
	      handle_define ();
	    }
	  else if (!strcmp ("if", yyp))
	    {
	      cond = cond_get_exp (0);
	      if (*outp != '\n')
		yyerrorp ("Condition too complex in %cif");
	      else
		handle_cond (cond);
	    }
	  else if (!strcmp ("ifdef", yyp))
	    {
	      deltrail ();
	      handle_cond (lookup_define (outp) != 0);
	    }
	  else if (!strcmp ("ifndef", yyp))
	    {
	      deltrail ();
	      handle_cond (!lookup_define (outp));
	    }
	  else if (!strcmp ("elif", yyp))
	    {
	      handle_elif ();
	    }
	  else if (!strcmp ("else", yyp))
	    {
	      handle_else ();
	    }
	  else if (!strcmp ("endif", yyp))
	    {
	      handle_endif ();
	    }
	  else if (!strcmp ("undef", yyp))
	    {
	      defn_t *d;

	      deltrail ();
	      if ((d = lookup_definition (outp)))
		{
		  d->flags |= DEF_IS_UNDEFINED;
		  d->flags &= ~DEF_IS_NOT_LOCAL;
		}
	      else
		{
		  add_define (outp, -1, " ");
		  d = lookup_definition (outp);
		  d->flags |= DEF_IS_UNDEFINED;
		  d->flags &= ~DEF_IS_NOT_LOCAL;
		}
	    }
	  else if (!strcmp ("echo", yyp))
	    {
	      fprintf (stderr, "echo at line %d of %s: %s\n", current_line,
		       current_file, outp);
	    }
	  else if (!strcmp ("include", yyp))
	    {
	      handle_include (outp);
	    }
	  else if (!strcmp ("pragma", yyp))
	    {
	      handle_pragma (outp);
	    }
	  else if (yyout)
	    {
	      if (!strcmp ("line", yyp))
		{
		  fprintf (yyout, "#line %d \"%s\"\n", current_line,
			   current_file);
		}
	      else
		{
		  if (sp_buf)
		    *oldoutp = sp_buf;
		  if (pragmas & PRAGMA_NOTE_CASE_START)
		    {
		      if (*yyp == '%')
			pragmas &= ~PRAGMA_NOTE_CASE_START;
		    }
		  fprintf (yyout, "%s\n", yyp - 1);
		}
	    }
	  else
	    {
	      char buff[200];
	      sprintf (buff, "Unrecognised %c directive : %s\n", ppchar, yyp);
	      yyerror (buff);
	    }
	}
      else if (c == '/')
	{
	  if ((c = *++yyp2) == '*')
	    {
	      if (yyout)
		fputs (yyp, yyout);
	      yyp2 = skip_comment (yyp2, 1);
	    }
	  else if (c == '/' && !yyout)
	    continue;
	  else if (yyout)
	    {
	      fprintf (yyout, "%s", yyp);
	    }
	}
      else if (yyout)
	{
	  fprintf (yyout, "%s", yyp);
	  if (pragmas & PRAGMA_NOTE_CASE_START)
	    {
	      static int line_to_print;

	      line_to_print = 0;

	      if (!in_c_case)
		{
		  while (isalunum (*yyp2))
		    yyp2++;
		  while (isspace (*yyp2))
		    yyp2++;
		  if (*yyp2 == ':')
		    {
		      in_c_case = 1;
		      yyp2++;
		    }
		}

	      if (in_c_case)
		{
		  while ((c = *yyp2++))
		    {
		      switch (c)
			{
			case '{':
			  {
			    if (!cquote && (++block_nest == 1))
			      line_to_print = 1;
			    break;
			  }

			case '}':
			  {
			    if (!cquote)
			      {
				if (--block_nest < 0)
				  yyerror ("Too many }'s");
			      }
			    break;
			  }

			case '"':
			  if (!(cquote & CHAR_QUOTE))
			    cquote ^= STRING_QUOTE;
			  break;

			case '\'':
			  if (!(cquote & STRING_QUOTE))
			    cquote ^= CHAR_QUOTE;
			  break;

			case '\\':
			  if (cquote && *yyp2)
			    yyp2++;
			  break;

			case '/':
			  if (!cquote)
			    {
			      if ((c = *yyp2) == '*')
				{
				  yyp2 = skip_comment (yyp2, 1);
				}
			      else if (c == '/')
				{
				  *(yyp2 - 1) = '\n';
				  *yyp2 = '\0';
				}
			    }
			  break;

			case ':':
			  if (!cquote && !block_nest)
			    yyerror
			      ("Case started before ending previous case with ;");
			  break;

			case ';':
			  if (!cquote && !block_nest)
			    in_c_case = 0;
			}
		    }
		}

	      if (line_to_print)
		fprintf (yyout, "#line %d \"%s\"\n", current_line + 1,
			 current_file);

	    }
	}
    }
  if (iftop)
    {
      ifstate_t *p = iftop;

      while (iftop)
	{
	  p = iftop;
	  iftop = p->next;
	  free (p);
	}
      yyerrorp ("Missing %cendif");
    }
  fclose (yyin);
  free (current_file);
  current_file = 0;
  nexpands = 0;
  if (inctop)
    {
      incstate *p = inctop;

      current_file = p->file;
      current_line = p->line;
      yyin = p->yyin;
      inctop = p->next;
      free ((char *) p);
      preprocess ();
    }
  else
    yyin = 0;
}

void
make_efun_tables ()
{
#define NUM_FILES     4
  static char *outfiles[NUM_FILES] = {
    HEADER_VECTOR, HEADER_OPCODE, HEADER_PROTOTYPE, HEADER_DEFINITION
  };
  FILE *files[NUM_FILES];
  int i;

  for (i = 0; i < NUM_FILES; i++)
    {
      fprintf (stderr, "Writing efun tables to %s ...\n", outfiles[i]);
      files[i] = fopen (outfiles[i], "w");
      if (!files[i])
	{
	  fprintf (stderr, "make_func: unable to open %s\n", outfiles[i]);
	  exit (-1);
	}
      fprintf (files[i], "/*\n\tThis file is automatically generated.\n");
      fprintf (files[i],
	       "\tDo not make any manual changes to this file.\n*/\n\n");
    }

  fprintf (files[0], "\n#include \"%s\"\n\n", HEADER_PROTOTYPE);
  fprintf (files[0], "\ntypedef void (*func_t) (void);\n\n");
  fprintf (files[0], "func_t efun_table[] = {\n");

  fprintf (files[1], "\n/* operators */\n\n");
  for (i = 0; i < op_code; i++)
    {
      fprintf (files[1], "#define %-30s %d\n", oper_codes[i], i + 1);
    }

  fprintf (files[1], "\n/* 1 arg efuns */\n#define BASE %d\n\n", op_code + 1);
  for (i = 0; i < efun1_code; i++)
    {
      fprintf (files[0], "\tf_%s,\n", efun1_names[i]);
      fprintf (files[1], "#define %-30s %d\n", efun1_codes[i],
	       i + op_code + 1);
      fprintf (files[2], "void f_%s(void);\n", efun1_names[i]);
    }

  fprintf (files[1], "\n/* efuns */\n#define ONEARG_MAX %d\n\n",
	   efun1_code + op_code + 1);
  for (i = 0; i < efun_code; i++)
    {
      fprintf (files[0], "\tf_%s,\n", efun_names[i]);
      fprintf (files[1], "#define %-30s %d\n", efun_codes[i],
	       i + op_code + efun1_code + 1);
      fprintf (files[2], "void f_%s(void);\n", efun_names[i]);
    }
  fprintf (files[0], "};\n");

  if (efun1_code + op_code >= 256)
    {
      fprintf (stderr,
	       "You have way too many efuns.  Contact the MudOS developers if you really need this many.\n");
    }
  if (efun_code >= 256)
    {
      fprintf (stderr,
	       "You have way too many efuns.  Contact the MudOS developers if you really need this many.\n");
    }
  fprintf (files[1], "\n/* efuns */\n#define NUM_OPCODES %d\n\n",
	   efun_code + efun1_code + op_code);

  /* Now sort the main_list */
  for (i = 0; i < num_buff; i++)
    {
      int j;
      for (j = 0; j < i; j++)
	if (strcmp (key[i], key[j]) < 0)
	  {
	    char *tmp;
	    tmp = key[i];
	    key[i] = key[j];
	    key[j] = tmp;
	    tmp = buf[i];
	    buf[i] = buf[j];
	    buf[j] = tmp;
	  }
    }

  /* Now display it... */
  fprintf (files[3], "{\n");
  for (i = 0; i < num_buff; i++)
    fprintf (files[3], "%s", buf[i]);
  fprintf (files[3], "\n};\nint efun_arg_types[] = {\n");
  for (i = 0; i < last_current_type; i++)
    {
      if (arg_types[i] == 0)
	fprintf (files[3], "0,\n");
      else
	fprintf (files[3], "%s,", ctype (arg_types[i]));
    }
  fprintf (files[3], "};\n");

  for (i = 0; i < NUM_FILES; i++)
    fclose (files[i]);
}

/*  handle_options()

    generate HEADER_OPTION
 */
static void
handle_options (char *fname)
{
  open_input_file (fname);
  ppchar = '#';
  preprocess ();

  create_option_defines ();
}

/*  handle_build_efuns()

    Generate the following files:
	HEADER_VECTOR
	HEADER_OPCODE
	HEADER_PROTOTYPE
	HEADER_DEFINITION
 */
static void
handle_build_efuns (const char *efun_spec)
{
  num_buff = op_code = efun_code = efun1_code = 0;

  open_input_file (efun_spec);
  yyparse ();
  make_efun_tables ();
}
