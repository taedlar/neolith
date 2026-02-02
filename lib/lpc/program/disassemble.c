#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "lpc/program.h"
#include "lpc/include/function.h"
#include "src/interpret.h"
#include "src/simul_efun.h"
#include "disassemble.h"

static char *
disassem_string (char *str)
{
  static char buf[30];
  char *b;
  int i;

  if (!str)
    return "0";

  b = buf;
  for (i = 0; i < 29; i++)
    {
      if (!str[i])
        break;
      if (str[i] == '\n')
        {
          *b++ = '\\';
          *b++ = 'n';
        }
      else
        {
          *b++ = str[i];
        }
    }
  *b++ = 0;
  return buf;
}

#define NUM_FUNS(pp)    ((pp)->num_functions_total)
#define NUM_FUNS_D(pp)  ((pp)->num_functions_defined)
#define VARS     prog->variable_names
#define NUM_VARS prog->num_variables_total
#define STRS     prog->strings
#define NUM_STRS prog->num_strings
#define CLSS     prog->classes

static int
short_compare (const void *a, const void *b)
{
  int x = *(unsigned short *) a;
  int y = *(unsigned short *) b;

  return x - y;
}

static char *pushes[] = { "string", "number", "global", "local" };

/**
 *  @brief Disassemble LPC bytecode to human-readable format.
 */
void disassemble (FILE *f, char *code, ptrdiff_t start, ptrdiff_t end, program_t *prog) {

  int i, j, instr, iarg /*, is_efun*/;
  unsigned short sarg;
  unsigned short offset;
  char *p, buff[256];
  int next_func;

  short *offsets;
  size_t offsets_tbl_size = NUM_FUNS_D(prog) * 2 * sizeof (short);

  if (!NUM_FUNS_D(prog))
    return;
  if (start == 0)
    {
      /* sort offsets of functions */
      offsets = (short *) malloc (offsets_tbl_size);
      for (i = 0; i < (int) NUM_FUNS_D(prog); i++)
        {
          if (prog->function_flags[prog->function_table[i].runtime_index]
              & (NAME_INHERITED | NAME_NO_CODE))
            offsets[i * 2] = (short)(end + 1); /* place at end */
          else
            offsets[i * 2] = prog->function_table[i].address;
          offsets[i * 2 + 1] = (short)i;
        }
#ifdef _SEQUENT_
      qsort ((void *) &offsets[0], NUM_FUNS_D(prog), sizeof (short) * 2, short_compare);
#else
      qsort ((char *) &offsets[0], NUM_FUNS_D(prog), sizeof (short) * 2, short_compare);
#endif
      next_func = 0;
    }
  else
    {
      offsets = 0;
      next_func = -1;
    }

  p = code + start;

  while ((p - code) < end)
    {
      if ((next_func >= 0) && ((p - code) >= offsets[next_func]))
        {
          fprintf (f, "\n;; Function %s\n",
                   prog->function_table[offsets[next_func + 1]].name);
          next_func += 2;
          if (next_func >= ((int) NUM_FUNS_D(prog)) * 2)
            next_func = -1;
        }

      fprintf (f, "%04x: ", (unsigned) (p - code));
      /*is_efun = (instr = EXTRACT_UCHAR (p)) >= BASE;*/
      instr = EXTRACT_UCHAR (p);
      p++;
      buff[0] = 0;
      sarg = 0;

      switch (instr)
        {
        case F_PUSH:
          fprintf (f, "push ");
          i = EXTRACT_UCHAR (p++);
          while (i--)
            {
              j = EXTRACT_UCHAR (p++);
              fprintf (f, "%s %i", pushes[(j & PUSH_WHAT) >> 6],
                       j & PUSH_MASK);
              if (i)
                fprintf (f, ", ");
              else
                break;
            }
          fprintf (f, "\n");
          continue;
          /* Single numeric arg */
        case F_BRANCH_NE:
        case F_BRANCH_GE:
        case F_BRANCH_LE:
        case F_BRANCH_EQ:
        case F_BRANCH:
        case F_BRANCH_WHEN_ZERO:
        case F_BRANCH_WHEN_NON_ZERO:
#ifdef F_LOR
        case F_LOR:
        case F_LAND:
#endif
          COPY_SHORT (&sarg, p);
          offset = (unsigned short)(p - code + sarg);
          sprintf (buff, "%04x (%04x)", (unsigned) sarg, (unsigned) offset);
          p += 2;
          break;

        case F_NEXT_FOREACH:
        case F_BBRANCH_LT:
          COPY_SHORT (&sarg, p);
          offset = (unsigned short)(p - code - sarg);
          sprintf (buff, "%04x (%04x)", (unsigned) sarg, (unsigned) offset);
          p += 2;
          break;

        case F_FOREACH:
          {
            char tmp[32];
            int flags = EXTRACT_UCHAR (p++);

            sprintf (buff, "(%s) %s %i", (flags & 4) ? "mapping" : "array",
                     (flags & 1) ? "global" : "local", EXTRACT_UCHAR (p++));
            if (flags & 4)
              {
                sprintf (tmp, ", %s %i", (flags & 2) ? "global" : "local",
                         EXTRACT_UCHAR (p++));
                strcat (buff, tmp);
              }
            break;
          }

        case F_BBRANCH_WHEN_ZERO:
        case F_BBRANCH_WHEN_NON_ZERO:
        case F_BBRANCH:
          COPY_SHORT (&sarg, p);
          offset = (unsigned short)(p - code - sarg);
          sprintf (buff, "%04x (%04x)", (unsigned) sarg, (unsigned) offset);
          p += 2;
          break;

#ifdef F_JUMP
        case F_JUMP:
#endif
#ifdef F_JUMP_WHEN_ZERO
        case F_JUMP_WHEN_ZERO:
        case F_JUMP_WHEN_NON_ZERO:
#endif
        case F_CATCH:
          COPY_SHORT (&sarg, p);
          sprintf (buff, "%04x", (unsigned) sarg);
          p += 2;
          break;

        case F_AGGREGATE:
        case F_AGGREGATE_ASSOC:
          COPY_SHORT (&sarg, p);
          sprintf (buff, "%d", (int) sarg);
          p += 2;
          break;

        case F_MEMBER:
        case F_MEMBER_LVALUE:
          sprintf (buff, "%d", (int) EXTRACT_UCHAR (p++));
          break;

        case F_EXPAND_VARARGS:
          {
            int which = EXTRACT_UCHAR (p++);
            if (which)
              {
                sprintf (buff, "%d from top of stack", which);
              }
            else
              {
                strcpy (buff, "top of stack");
              }
          }
          break;

        case F_NEW_EMPTY_CLASS:
        case F_NEW_CLASS:
          {
            int which = EXTRACT_UCHAR (p++);

            strcpy (buff, STRS[CLSS[which].name]);
            break;
          }

        case F_CALL_FUNCTION_BY_ADDRESS:
          COPY_SHORT (&sarg, p);
          p += 3;
          if (sarg < NUM_FUNS(prog))
            sprintf (buff, "%-12s %5d", function_name (prog, sarg),
                     (int) sarg);
          else
            sprintf (buff, "<out of range %d>", (int) sarg);
          break;

        case F_CALL_INHERITED:
          {
            program_t *newprog;

            newprog = (prog->inherit + EXTRACT_UCHAR (p++))->prog;
            COPY_SHORT (&sarg, p);
            p += 3;
            if (sarg < newprog->num_functions_total)
              sprintf (buff, "%30s::%-12s %5d", newprog->name,
                       function_name (newprog, sarg), (int) sarg);
            else
              sprintf (buff, "<out of range in %30s - %d>", newprog->name,
                       (int) sarg);
            break;
          }
        case F_GLOBAL_LVALUE:
        case F_GLOBAL:
          if ((unsigned) (iarg = EXTRACT_UCHAR (p)) < NUM_VARS)
            sprintf (buff, "%s", variable_name (prog, iarg));
          else
            sprintf (buff, "<out of range %d>", iarg);
          p++;
          break;

        case F_LOOP_INCR:
          sprintf (buff, "LV%d", EXTRACT_UCHAR (p));
          p++;
          break;
        case F_WHILE_DEC:
          COPY_SHORT (&sarg, p + 1);
          offset = (unsigned short)(p - code - sarg);
          sprintf (buff, "LV%d--, branch %04x (%04x)", EXTRACT_UCHAR (p),
                   (unsigned) sarg, (unsigned) offset);
          p += 3;
          break;
        case F_TRANSFER_LOCAL:
        case F_LOCAL:
        case F_LOCAL_LVALUE:
        case F_VOID_ASSIGN_LOCAL:
          sprintf (buff, "LV%d", EXTRACT_UCHAR (p));
          p++;
          break;
        case F_LOOP_COND_NUMBER:
          i = EXTRACT_UCHAR (p++);
          COPY_INT (&iarg, p);
          p += 4;
          COPY_SHORT (&sarg, p);
          offset = (unsigned short)(p - code - sarg);
          p += 2;
          sprintf (buff, "LV%d < %d bbranch_when_non_zero %04x (%04x)",
                   i, iarg, sarg, offset);
          break;
        case F_LOOP_COND_LOCAL:
          i = EXTRACT_UCHAR (p++);
          iarg = *p++;
          COPY_SHORT (&sarg, p);
          offset = (unsigned short)(p - code - sarg);
          p += 2;
          sprintf (buff, "LV%d < LV%d bbranch_when_non_zero %04x (%04x)",
                   i, iarg, sarg, offset);
          break;
        case F_STRING:
          COPY_SHORT (&sarg, p);
          if (sarg < NUM_STRS)
            sprintf (buff, "\"%s\"", disassem_string (STRS[sarg]));
          else
            sprintf (buff, "<out of range %d>", (int) sarg);
          p += 2;
          break;
        case F_SHORT_STRING:
          if (EXTRACT_UCHAR (p) < NUM_STRS)
            sprintf (buff, "\"%s\"",
                     disassem_string (STRS[EXTRACT_UCHAR (p)]));
          else
            sprintf (buff, "<out of range %d>", EXTRACT_UCHAR (p));
          p++;
          break;
        case F_SIMUL_EFUN:
          COPY_SHORT (&sarg, p);
          sprintf (buff, "\"%s\" %d", simuls[sarg].func->name, p[2]);
          p += 3; /* index(2), num_args(1)*/
          break;

        case F_FUNCTION_CONSTRUCTOR:
          switch (EXTRACT_UCHAR (p++))
            {
            case FP_SIMUL:
              LOAD_SHORT (sarg, p);
              sprintf (buff, "<simul_efun> \"%s\"", simuls[sarg].func->name);
              break;
            case FP_EFUN:
              LOAD_SHORT (sarg, p);
              sprintf (buff, "<efun> %s", instrs[sarg].name);
              break;
            case FP_LOCAL:
              LOAD_SHORT (sarg, p);
              if (sarg < NUM_FUNS(prog))
                sprintf (buff, "<local_fun> %s", function_name (prog, sarg));
              else
                sprintf (buff, "<local_fun> <out of range %d>", (int) sarg);
              break;
            case FP_FUNCTIONAL:
            case FP_FUNCTIONAL | FP_NOT_BINDABLE:
              sprintf (buff, "<functional, %d args>\nCode:", (int) p[0]);
              p += 3;
              break;
            case FP_ANONYMOUS:
            case FP_ANONYMOUS | FP_NOT_BINDABLE:
              COPY_SHORT (&sarg, &p[2]);
              sprintf (buff,
                       "<anonymous function, %d args, %d locals, ends at %04x>\nCode:",
                       (int) p[0], (int) p[1],
                       (int) (p + 3 + sarg - code));
              p += 4;
              break;
            }
          break;

        case F_NUMBER:
          COPY_INT (&iarg, p);
          sprintf (buff, "%d", iarg);
          p += 4;
          break;

        case F_LONG:
          {
            int64_t larg;
            COPY_LONG (&larg, p);
#ifdef _WIN32
            sprintf (buff, "%I64d", larg);
#else
            sprintf (buff, "%lld", (long long)larg);
#endif
            p += 8;
            break;
          }

        case F_REAL:
          {
            double farg;

            COPY_FLOAT (&farg, p);
            sprintf (buff, "%lf", farg);
            p += 8; /* [NEOLITH-EXTENSION] always use double-precision */
            break;
          }

        case F_SSCANF:
        case F_PARSE_COMMAND:
        case F_BYTE:
          sprintf (buff, "%d", EXTRACT_UCHAR (p));
          p++;
          break;

        case F_NBYTE:
          sprintf (buff, "-%d", EXTRACT_UCHAR (p));
          p++;
          break;

        case F_SWITCH:
          {
            unsigned char ttype;
            unsigned short stable, etable, def;
            char *parg;

            ttype = EXTRACT_UCHAR (p);
            ((char *) &stable)[0] = p[1];
            ((char *) &stable)[1] = p[2];
            ((char *) &etable)[0] = p[3];
            ((char *) &etable)[1] = p[4];
            ((char *) &def)[0] = p[5];
            ((char *) &def)[1] = p[6];
            fprintf (f, "switch\n");
            fprintf (f, "      type: %02x table: %04x-%04x deflt: %04x\n",
                     (unsigned) ttype, (unsigned) stable,
                     (unsigned) etable, (unsigned) def);
            /* recursively disassemble stuff in switch */
            disassemble (f, code, p - code + 7, stable, prog);

            /* now print out table - ugly... */
            fprintf (f, "      switch table (for %04x)\n",
                     (unsigned) (p - code - 1));
            if (ttype == 0xfe)
              ttype = 0;	/* direct lookup */
            else if (ttype >> 4 == 0xf)
              ttype = 1;	/* normal int */
            else
              ttype = 2;	/* string */

            p = code + stable;
            if (ttype == 0)
              {
                i = 0;
                while (p < code + etable - 4)
                  {
                    COPY_SHORT (&sarg, p);
                    fprintf (f, "\t%2d: %04x\n", i++, (unsigned) sarg);
                    p += 2;
                  }
                COPY_INT (&iarg, p);
                fprintf (f, "\tminval = %d\n", iarg);
                p += 4;
              }
            else
              {
                while (p < code + etable)
                  {
                    COPY_PTR (&parg, p);
                    COPY_SHORT (&sarg, p + sizeof (char *));
                    if (ttype == 1 || !parg)
                      {
                        fprintf (f, "\t%-4lld\t%04lx\n", (intptr_t) parg, (long) sarg);
                      }
                    else
                      {
                        fprintf (f, "\t\"%s\"\t%04lx\n", disassem_string (parg), (long) sarg);
                      }
                    p += 2 + sizeof (char *);
                  }
              }
            continue;
          }
        case F_EFUNV:
          sprintf (buff, "%d", EXTRACT_UCHAR (p++));
          instr = EXTRACT_UCHAR (p++) + ONEARG_MAX;
          break;
        case F_EFUN0:
        case F_EFUN1:
        case F_EFUN2:
        case F_EFUN3:
          if (instrs[instr].min_arg != instrs[instr].max_arg)
            {
              sprintf (buff, "%d", instr - F_EFUN0);
            }
          instr = EXTRACT_UCHAR (p++) + ONEARG_MAX;
          break;
        }
      fprintf (f, "%s %s\n", query_opcode_name (instr), buff);
    }

  if (offsets)
    free (offsets);
}

#define INCLUDE_DEPTH 10

void
dump_line_numbers (FILE * f, program_t * prog)
{
  unsigned short *fi;
  unsigned char *li_start;
  unsigned char *li_end;
  unsigned char *li;
  int addr;
  int sz;
  short s;

  if (!prog->line_info)
    {
      fprintf (f, "No line number information available.\n");
      return;
    }

  fi = prog->file_info;
  li_end = (unsigned char *) (((char *) fi) + fi[0]);
  li_start = (unsigned char *) (fi + fi[1]);

  fi += 2;
  fprintf (f, "\nabsolute line -> (file, line) table:\n");
  while (fi < (unsigned short *) li_start)
    {
      fprintf (f, "%i lines from %i [%s]\n", (int) fi[0], (int) fi[1],
               prog->strings[fi[1] - 1]);
      fi += 2;
    }

  li = li_start;
  addr = 0;
  fprintf (f, "\naddress -> absolute line table:\n");
  while (li < li_end)
    {
      sz = *li++;
      COPY_SHORT (&s, li);
      li += 2;
      fprintf (f, "%4x-%4x: %i\n", addr, addr + sz - 1, (int) s);
      addr += sz;
    }
}

