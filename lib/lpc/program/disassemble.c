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
void disassemble (FILE *f, char *code, int start, int end, program_t *prog) {

  int i, j, instr, iarg /*, is_efun*/;
  unsigned short sarg;
  unsigned short offset;
  char *pc, buff[256];
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
            offsets[i * 2] = end + 1;
          else
            offsets[i * 2] = prog->function_table[i].address;
          offsets[i * 2 + 1] = i;
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

  pc = code + start;

  while ((pc - code) < end)
    {
      if ((next_func >= 0) && ((pc - code) >= offsets[next_func]))
        {
          fprintf (f, "\n;; Function %s\n",
                   prog->function_table[offsets[next_func + 1]].name);
          next_func += 2;
          if (next_func >= ((int) NUM_FUNS_D(prog)) * 2)
            next_func = -1;
        }

      fprintf (f, "%04x: ", (unsigned) (pc - code));
      /*is_efun = (instr = EXTRACT_UCHAR (pc)) >= BASE;*/
      instr = EXTRACT_UCHAR (pc);
      pc++;
      buff[0] = 0;
      sarg = 0;

      switch (instr)
        {
        case F_PUSH:
          fprintf (f, "push ");
          i = EXTRACT_UCHAR (pc++);
          while (i--)
            {
              j = EXTRACT_UCHAR (pc++);
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
          COPY_SHORT (&sarg, pc);
          offset = (pc - code) + (unsigned short) sarg;
          sprintf (buff, "%04x (%04x)", (unsigned) sarg, (unsigned) offset);
          pc += 2;
          break;

        case F_NEXT_FOREACH:
        case F_BBRANCH_LT:
          COPY_SHORT (&sarg, pc);
          offset = (pc - code) - (unsigned short) sarg;
          sprintf (buff, "%04x (%04x)", (unsigned) sarg, (unsigned) offset);
          pc += 2;
          break;

        case F_FOREACH:
          {
            char tmp[32];
            int flags = EXTRACT_UCHAR (pc++);

            sprintf (buff, "(%s) %s %i", (flags & 4) ? "mapping" : "array",
                     (flags & 1) ? "global" : "local", EXTRACT_UCHAR (pc++));
            if (flags & 4)
              {
                sprintf (tmp, ", %s %i", (flags & 2) ? "global" : "local",
                         EXTRACT_UCHAR (pc++));
                strcat (buff, tmp);
              }
            break;
          }

        case F_BBRANCH_WHEN_ZERO:
        case F_BBRANCH_WHEN_NON_ZERO:
        case F_BBRANCH:
          COPY_SHORT (&sarg, pc);
          offset = (pc - code) - (unsigned short) sarg;
          sprintf (buff, "%04x (%04x)", (unsigned) sarg, (unsigned) offset);
          pc += 2;
          break;

#ifdef F_JUMP
        case F_JUMP:
#endif
#ifdef F_JUMP_WHEN_ZERO
        case F_JUMP_WHEN_ZERO:
        case F_JUMP_WHEN_NON_ZERO:
#endif
        case F_CATCH:
          COPY_SHORT (&sarg, pc);
          sprintf (buff, "%04x", (unsigned) sarg);
          pc += 2;
          break;

        case F_AGGREGATE:
        case F_AGGREGATE_ASSOC:
          COPY_SHORT (&sarg, pc);
          sprintf (buff, "%d", (int) sarg);
          pc += 2;
          break;

        case F_MEMBER:
        case F_MEMBER_LVALUE:
          sprintf (buff, "%d", (int) EXTRACT_UCHAR (pc++));
          break;

        case F_EXPAND_VARARGS:
          {
            int which = EXTRACT_UCHAR (pc++);
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
            int which = EXTRACT_UCHAR (pc++);

            strcpy (buff, STRS[CLSS[which].name]);
            break;
          }

        case F_CALL_FUNCTION_BY_ADDRESS:
          COPY_SHORT (&sarg, pc);
          pc += 3;
          if (sarg < NUM_FUNS(prog))
            sprintf (buff, "%-12s %5d", function_name (prog, sarg),
                     (int) sarg);
          else
            sprintf (buff, "<out of range %d>", (int) sarg);
          break;

        case F_CALL_INHERITED:
          {
            program_t *newprog;

            newprog = (prog->inherit + EXTRACT_UCHAR (pc++))->prog;
            COPY_SHORT (&sarg, pc);
            pc += 3;
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
          if ((unsigned) (iarg = EXTRACT_UCHAR (pc)) < NUM_VARS)
            sprintf (buff, "%s", variable_name (prog, iarg));
          else
            sprintf (buff, "<out of range %d>", iarg);
          pc++;
          break;

        case F_LOOP_INCR:
          sprintf (buff, "LV%d", EXTRACT_UCHAR (pc));
          pc++;
          break;
        case F_WHILE_DEC:
          COPY_SHORT (&sarg, pc + 1);
          offset = (pc - code) - (unsigned short) sarg;
          sprintf (buff, "LV%d--, branch %04x (%04x)", EXTRACT_UCHAR (pc),
                   (unsigned) sarg, (unsigned) offset);
          pc += 3;
          break;
        case F_TRANSFER_LOCAL:
        case F_LOCAL:
        case F_LOCAL_LVALUE:
        case F_VOID_ASSIGN_LOCAL:
          sprintf (buff, "LV%d", EXTRACT_UCHAR (pc));
          pc++;
          break;
        case F_LOOP_COND_NUMBER:
          i = EXTRACT_UCHAR (pc++);
          COPY_INT (&iarg, pc);
          pc += 4;
          COPY_SHORT (&sarg, pc);
          offset = (pc - code) - (unsigned short) sarg;
          pc += 2;
          sprintf (buff, "LV%d < %d bbranch_when_non_zero %04x (%04x)",
                   i, iarg, sarg, offset);
          break;
        case F_LOOP_COND_LOCAL:
          i = EXTRACT_UCHAR (pc++);
          iarg = *pc++;
          COPY_SHORT (&sarg, pc);
          offset = (pc - code) - (unsigned short) sarg;
          pc += 2;
          sprintf (buff, "LV%d < LV%d bbranch_when_non_zero %04x (%04x)",
                   i, iarg, sarg, offset);
          break;
        case F_STRING:
          COPY_SHORT (&sarg, pc);
          if (sarg < NUM_STRS)
            sprintf (buff, "\"%s\"", disassem_string (STRS[sarg]));
          else
            sprintf (buff, "<out of range %d>", (int) sarg);
          pc += 2;
          break;
        case F_SHORT_STRING:
          if (EXTRACT_UCHAR (pc) < NUM_STRS)
            sprintf (buff, "\"%s\"",
                     disassem_string (STRS[EXTRACT_UCHAR (pc)]));
          else
            sprintf (buff, "<out of range %d>", EXTRACT_UCHAR (pc));
          pc++;
          break;
        case F_SIMUL_EFUN:
          COPY_SHORT (&sarg, pc);
          sprintf (buff, "\"%s\" %d", simuls[sarg].func->name, pc[2]);
          pc += 3;
          break;

        case F_FUNCTION_CONSTRUCTOR:
          switch (EXTRACT_UCHAR (pc++))
            {
            case FP_SIMUL:
              LOAD_SHORT (sarg, pc);
              sprintf (buff, "<simul_efun> \"%s\"", simuls[sarg].func->name);
              break;
            case FP_EFUN:
              LOAD_SHORT (sarg, pc);
              sprintf (buff, "<efun> %s", instrs[sarg].name);
              break;
            case FP_LOCAL:
              LOAD_SHORT (sarg, pc);
              if (sarg < NUM_FUNS(prog))
                sprintf (buff, "<local_fun> %s", function_name (prog, sarg));
              else
                sprintf (buff, "<local_fun> <out of range %d>", (int) sarg);
              break;
            case FP_FUNCTIONAL:
            case FP_FUNCTIONAL | FP_NOT_BINDABLE:
              sprintf (buff, "<functional, %d args>\nCode:", (int) pc[0]);
              pc += 3;
              break;
            case FP_ANONYMOUS:
            case FP_ANONYMOUS | FP_NOT_BINDABLE:
              COPY_SHORT (&sarg, &pc[2]);
              sprintf (buff,
                       "<anonymous function, %d args, %d locals, ends at %04x>\nCode:",
                       (int) pc[0], (int) pc[1],
                       (int) (pc + 3 + sarg - code));
              pc += 4;
              break;
            }
          break;

        case F_NUMBER:
          COPY_INT (&iarg, pc);
          sprintf (buff, "%d", iarg);
          pc += 4;
          break;

        case F_REAL:
          {
            float farg;

            COPY_FLOAT (&farg, pc);
            sprintf (buff, "%f", farg);
            pc += 4;
            break;
          }

        case F_SSCANF:
        case F_PARSE_COMMAND:
        case F_BYTE:
          sprintf (buff, "%d", EXTRACT_UCHAR (pc));
          pc++;
          break;

        case F_NBYTE:
          sprintf (buff, "-%d", EXTRACT_UCHAR (pc));
          pc++;
          break;

        case F_SWITCH:
          {
            unsigned char ttype;
            unsigned short stable, etable, def;
            char *parg;

            ttype = EXTRACT_UCHAR (pc);
            ((char *) &stable)[0] = pc[1];
            ((char *) &stable)[1] = pc[2];
            ((char *) &etable)[0] = pc[3];
            ((char *) &etable)[1] = pc[4];
            ((char *) &def)[0] = pc[5];
            ((char *) &def)[1] = pc[6];
            fprintf (f, "switch\n");
            fprintf (f, "      type: %02x table: %04x-%04x deflt: %04x\n",
                     (unsigned) ttype, (unsigned) stable,
                     (unsigned) etable, (unsigned) def);
            /* recursively disassemble stuff in switch */
            disassemble (f, code, pc - code + 7, stable, prog);

            /* now print out table - ugly... */
            fprintf (f, "      switch table (for %04x)\n",
                     (unsigned) (pc - code - 1));
            if (ttype == 0xfe)
              ttype = 0;	/* direct lookup */
            else if (ttype >> 4 == 0xf)
              ttype = 1;	/* normal int */
            else
              ttype = 2;	/* string */

            pc = code + stable;
            if (ttype == 0)
              {
                i = 0;
                while (pc < code + etable - 4)
                  {
                    COPY_SHORT (&sarg, pc);
                    fprintf (f, "\t%2d: %04x\n", i++, (unsigned) sarg);
                    pc += 2;
                  }
                COPY_INT (&iarg, pc);
                fprintf (f, "\tminval = %d\n", iarg);
                pc += 4;
              }
            else
              {
                while (pc < code + etable)
                  {
                    COPY_PTR (&parg, pc);
                    COPY_SHORT (&sarg, pc + sizeof (char *));
                    if (ttype == 1 || !parg)
                      {
                        fprintf (f, "\t%-4ld\t%04lx\n", (intptr_t) parg, (intptr_t) sarg);
                      }
                    else
                      {
                        fprintf (f, "\t\"%s\"\t%04lx\n",
                                 disassem_string (parg), (intptr_t) sarg);
                      }
                    pc += 2 + sizeof (char *);
                  }
              }
            continue;
          }
        case F_EFUNV:
          sprintf (buff, "%d", EXTRACT_UCHAR (pc++));
          instr = EXTRACT_UCHAR (pc++) + ONEARG_MAX;
          break;
        case F_EFUN0:
        case F_EFUN1:
        case F_EFUN2:
        case F_EFUN3:
          if (instrs[instr].min_arg != instrs[instr].max_arg)
            {
              sprintf (buff, "%d", instr - F_EFUN0);
            }
          instr = EXTRACT_UCHAR (pc++) + ONEARG_MAX;
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

