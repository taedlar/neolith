#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

/* 
 * code generator for runtime LPC code
 */
#include "src/std.h"
#include "lpc/include/function.h"
#include "icode.h"
#include "generate.h"
#include "lpc/compiler.h"

static void ins_real (double);
static void ins_short (short val);
static void upd_short (ptrdiff_t offset, short val);
static void ins_byte (BYTE byte);
static void upd_byte (ptrdiff_t offset, BYTE byte);
static void write_number (int number);
static void write_long_number (int64_t number);
static short read_short (ptrdiff_t offset);
static void ins_int (int);
static void ins_long (int64_t);
void i_generate_node (parse_node_t *);
static void i_generate_if_branch (parse_node_t *, int);
static void i_generate_loop (int, parse_node_t *, parse_node_t *,
                             parse_node_t *);
static void i_update_branch_list (parse_node_t *);
static int try_to_push (int, int);

/*
   this variable is used to properly adjust the 'break_sp' stack in
   the event a 'continue' statement is issued from inside a 'switch'.
*/
static int foreach_depth = 0;

static ptrdiff_t current_forward_branch;
static int current_num_values;

static size_t last_size_generated;
static int line_being_generated;

static int push_state;
static int push_start;

static parse_node_t *branch_list[3];

/**
 *  @brief Insert a double precision floating point number into the program code.
 *  In original LPMud and MudOS, this was a single precision float.
 *  Neolith has extended to use double precision by default for better accuracy.
 *  Note that the bytecode format is not compatible with MudOS anymore since a
 *  double is 8 bytes while a float is 4 bytes. (The load_binary() function does
 *  not presume such compatibility anyway.)
 */
static void ins_real (double l) {

  double f = (double) l;

  if (prog_code + 8 > prog_code_max)
    {
      mem_block_t *mbp = &mem_block[current_block];

      UPDATE_PROGRAM_SIZE;
      realloc_mem_block (mbp, mbp->current_size * 2);

      prog_code = mbp->block + mbp->current_size;
      prog_code_max = mbp->block + mbp->max_size;
    }
  STORE_FLOAT (prog_code, f); /* [NEOLITH-EXTENSION] always use double-precision */
}

/*
 * Store a 2 byte number. It is stored in such a way as to be sure
 * that correct byte order is used, regardless of machine architecture.
 * Also beware that some machines can't write a word to odd addresses.
 */
static void ins_short (short val) {
  short l = (short) val;
  if (prog_code + 2 > prog_code_max)
    {
      mem_block_t *mbp = &mem_block[current_block];
      UPDATE_PROGRAM_SIZE;
      realloc_mem_block (mbp, mbp->current_size * 2);

      prog_code = mbp->block + mbp->current_size;
      prog_code_max = mbp->block + mbp->max_size;
    }
  STORE_SHORT (prog_code, l);
}

static short read_short (ptrdiff_t offset) {
  short l;

  COPY_SHORT (&l, mem_block[current_block].block + offset);
  return l;
}

/**
 * Store a 4 byte number. It is stored in such a way as to be sure
 * that correct byte order is used, regardless of machine architecture.
 */
static void ins_int (int l) {

  if (prog_code + 4 > prog_code_max)
    {
      mem_block_t *mbp = &mem_block[current_block];
      UPDATE_PROGRAM_SIZE;
      realloc_mem_block (mbp, mbp->current_size * 2);

      prog_code = mbp->block + mbp->current_size;
      prog_code_max = mbp->block + mbp->max_size;
    }
  STORE_INT (prog_code, l);
}

/**
 * Store an 8 byte number. It is stored in such a way as to be sure
 * that correct byte order is used, regardless of machine architecture.
 */
static void ins_long (int64_t l) {

  if (prog_code + 8 > prog_code_max)
    {
      mem_block_t *mbp = &mem_block[current_block];
      UPDATE_PROGRAM_SIZE;
      realloc_mem_block (mbp, mbp->current_size * 2);

      prog_code = mbp->block + mbp->current_size;
      prog_code_max = mbp->block + mbp->max_size;
    }
  STORE_LONG (prog_code, l);
}

/*
 * Store a integer pointer. It is stored in such a way as to be sure
 * that correct byte order is used, regardless of machine architecture.
 */
static void ins_intptr (intptr_t l) {
#if UINTPTR_MAX == UINT32_MAX
  if (prog_code + 4 > prog_code_max)
#elif UINTPTR_MAX == UINT64_MAX
  if (prog_code + 8 > prog_code_max)
#else
#error only supports pointer size of 4 or 8 bytes
#endif
    {
      mem_block_t *mbp = &mem_block[current_block];
      UPDATE_PROGRAM_SIZE;
      realloc_mem_block (mbp, mbp->current_size * 2);

      prog_code = mbp->block + mbp->current_size;
      prog_code_max = mbp->block + mbp->max_size;
    }
#if UINTPTR_MAX == UINT32_MAX
  STORE4 (prog_code, l);
#elif UINTPTR_MAX == UINT64_MAX
  STORE8 (prog_code, l);
#else
#error only supports pointer size of 4 or 8 bytes
#endif
}

static void upd_short (ptrdiff_t offset, short val) {
  IF_DEBUG (UPDATE_PROGRAM_SIZE);
  DEBUG_CHECK2 (offset > CURRENT_PROGRAM_SIZE,
                "patch offset %x larger than current program size %x.\n",
                offset, CURRENT_PROGRAM_SIZE);
  COPY_SHORT (mem_block[current_block].block + offset, &val);
}

/**
 *  @brief Insert a byte into the program code.
 *  Grows the current memory block if necessary.
 */
static void ins_byte (BYTE b) {
  if (prog_code == prog_code_max)
    {
      mem_block_t *mbp = &mem_block[current_block];
      UPDATE_PROGRAM_SIZE;
      realloc_mem_block (mbp, mbp->current_size * 2);

      prog_code = mbp->block + mbp->current_size;
      prog_code_max = mbp->block + mbp->max_size;
    }
  *prog_code++ = b;
}

/**
 * @brief Update a byte in the program code at the given offset.
 */
static void upd_byte (ptrdiff_t offset, BYTE b) {
  IF_DEBUG (UPDATE_PROGRAM_SIZE);
  DEBUG_CHECK2 (offset > CURRENT_PROGRAM_SIZE,
                "patch offset %x larger than current program size %x.\n",
                offset, CURRENT_PROGRAM_SIZE);
  mem_block[current_block].block[offset] = b;
}

/**
 * @brief End a series of push operations.
 */
static void end_pushes (void) {
  if (push_state)
    {
      if (push_state > 1)
        upd_byte (push_start, (BYTE)push_state);
      push_state = 0;
    }
}

/**
 * @brief Initialize a push operation sequence.
 */
static void initialize_push (void) {

  int what = mem_block[current_block].block[push_start];
  int arg = mem_block[current_block].block[push_start + 1];

  prog_code = mem_block[current_block].block + push_start;
  ins_byte (F_PUSH);
  push_start++;			/* now points to the zero here */
  ins_byte (0);

  switch (what)
    {
    case F_CONST0:
      ins_byte (PUSH_NUMBER | 0);
      break;
    case F_CONST1:
      ins_byte (PUSH_NUMBER | 1);
      break;
    case F_BYTE:
      ins_byte ((BYTE)(PUSH_NUMBER | arg));
      break;
    case F_SHORT_STRING:
      ins_byte ((BYTE)(PUSH_STRING | arg));
      break;
    case F_LOCAL:
      ins_byte ((BYTE)(PUSH_LOCAL | arg));
      break;
    case F_GLOBAL:
      ins_byte ((BYTE)(PUSH_GLOBAL | arg));
      break;
    }
}

/**
 * Generate the code to push a number on the stack.
 * This varies since there are several opcodes (for
 * optimizing speed and/or size).
 */
static void write_small_number (int val) {
  if (try_to_push (PUSH_NUMBER, val))
    return;
  ins_byte (F_BYTE);
  ins_byte ((BYTE)val);
}

static void write_number (int val) {

  if ((val & ~0xff) == 0)
    write_small_number (val);
  else
    {
      end_pushes ();
      if (val < 0 && val > -256)
        {
          ins_byte (F_NBYTE);
          ins_byte ((BYTE)(-val));
        }
      else
        {
          ins_byte (F_NUMBER);
          ins_int (val);
        }
    }
}

static void write_long_number (int64_t val) {

  /* Check if it fits in 32-bit range, use F_NUMBER for efficiency */
  if (val >= INT32_MIN && val <= INT32_MAX)
    {
      write_number ((int)val);
    }
  else
    {
      /* Requires full 64-bit encoding */
      end_pushes ();
      ins_byte (F_LONG);
      ins_long (val);
    }
}

static void generate_expr_list (parse_node_t * expr) {

  parse_node_t *pn;
  int n, flag;

  if (!expr)
    return;
  pn = expr;
  flag = n = 0;
  do
    {
      if (pn->type & 1)
        flag = 1;
      i_generate_node (pn->v.expr);
      n++;
    }
  while ((pn = pn->r.expr));

  if (flag)
    {
      pn = expr;
      do
        {
          n--;
          if (pn->type & 1)
            {
              end_pushes ();
              ins_byte (F_EXPAND_VARARGS);
              ins_byte ((BYTE)n);
            }
        }
      while ((pn = pn->r.expr));
    }
}

static void generate_lvalue_list (parse_node_t * expr) {

  while ((expr = expr->r.expr))
    {
      i_generate_node (expr->l.expr);
      end_pushes ();
      ins_byte (F_VOID_ASSIGN);
    }
}

/**
 * @brief Switch to generating code for a new source line.
 * This function updates the line number information
 * for the code generator.
 * @param line The new source line number.
 */
static void switch_to_line (int line) {

  ptrdiff_t sz = CURRENT_PROGRAM_SIZE - last_size_generated;
  short s;
  unsigned char *p;

  /* should be fixed later */
  if (current_block != A_PROGRAM)
    return;

  if (sz)
    {
      s = (short)line_being_generated;

      last_size_generated += sz;
      while (sz > 255)
        {
          p = (unsigned char *) allocate_in_mem_block (A_LINENUMBERS, 3);
          *p++ = 255;
          STORE_SHORT (p, s);
          sz -= 255;
        }
      p = (unsigned char *) allocate_in_mem_block (A_LINENUMBERS, 3);
      *p++ = (unsigned char)sz;
      STORE_SHORT (p, s);
    }
  line_being_generated = line;
}

/**
 * Try to encode a push operation.
 * This function tries to encode a push operation using
 * a special compact form. If this is not possible,
 * it returns 0, otherwise 1.
 * @param kind The kind of push (PUSH_STRING, PUSH_LOCAL, etc).
 * @param value The value to push.
 * @return 1 if the push was encoded, 0 otherwise.
 */
static int try_to_push (int kind, int value) {

  if (push_state)
    {
      if (value <= PUSH_MASK)
        {
          if (push_state == 1)
            initialize_push ();
          push_state++;
          ins_byte ((BYTE)(kind | value));
          if (push_state == 255)
            end_pushes ();
          return 1;
        }
      else
        end_pushes ();
    }
  else if (value <= PUSH_MASK)
    {
      push_start = (int)CURRENT_PROGRAM_SIZE;
      push_state = 1;
      switch (kind)
        {
        case PUSH_STRING:
          ins_byte (F_SHORT_STRING);
          break;
        case PUSH_LOCAL:
          ins_byte (F_LOCAL);
          break;
        case PUSH_GLOBAL:
          ins_byte (F_GLOBAL);
          break;
        case PUSH_NUMBER:
          if (value == 0)
            {
              ins_byte (F_CONST0);
              return 1;
            }
          else if (value == 1)
            {
              ins_byte (F_CONST1);
              return 1;
            }
          ins_byte (F_BYTE);
        }
      ins_byte ((BYTE)value);
      return 1;
    }
  return 0;
}

/**
 *  @brief Code generator for parse trees.
 *  This is a recursive function that generates code for
 *  the parse tree given as argument.
 * 	@param expr The parse tree node to generate code for.
 */
void i_generate_node (parse_node_t * expr) {

  if (!expr)
    return;

  if (expr->line && expr->line != line_being_generated)
    switch_to_line (expr->line);
  switch (expr->kind)
    {
    case NODE_TERNARY_OP:
      i_generate_node (expr->l.expr);
      expr = expr->r.expr;
      /* fall through */
    case NODE_BINARY_OP:
      i_generate_node (expr->l.expr);
      /* fall through */
    case NODE_UNARY_OP:
      i_generate_node (expr->r.expr);
      /* fall through */
    case NODE_OPCODE:
      end_pushes ();
      ins_byte ((BYTE)expr->v.number);
      break;
    case NODE_TERNARY_OP_1:
      i_generate_node (expr->l.expr);
      expr = expr->r.expr;
      /* fall through */
    case NODE_BINARY_OP_1:
      i_generate_node (expr->l.expr);
      i_generate_node (expr->r.expr);
      end_pushes ();
      ins_byte ((BYTE)expr->v.number);
      ins_byte ((BYTE)expr->type);
      break;
    case NODE_UNARY_OP_1:
      i_generate_node (expr->r.expr);
      /* fall through */
    case NODE_OPCODE_1:
      if (expr->v.number == F_LOCAL)
        {
          if (try_to_push (PUSH_LOCAL, (int)expr->l.number))
            break;
        }
      else if (expr->v.number == F_GLOBAL)
        {
          if (try_to_push (PUSH_GLOBAL, (int)expr->l.number))
            break;
        }
      end_pushes ();
      ins_byte ((BYTE)expr->v.number);
      ins_byte ((BYTE)expr->l.number);
      break;
    case NODE_OPCODE_2:
      end_pushes ();
      ins_byte ((BYTE)expr->v.number);
      ins_byte ((BYTE)expr->l.number);
      if (expr->v.number == F_LOOP_COND_NUMBER)
        ins_int ((int)expr->r.number);
      else
        ins_byte ((BYTE)expr->r.number);
      break;
    case NODE_RETURN:
      {
        int n;
        n = foreach_depth;
        end_pushes ();
        while (n--)
          ins_byte (F_EXIT_FOREACH);

        if (expr->r.expr)
          {
            i_generate_node (expr->r.expr);
            end_pushes ();
            ins_byte (F_RETURN);
          }
        else
          ins_byte (F_RETURN_ZERO);
        break;
      }
    case NODE_STRING:
      if (try_to_push (PUSH_STRING, (int)expr->v.number))
        break;
      if (expr->v.number <= 0xff)
        {
          ins_byte (F_SHORT_STRING);
          ins_byte ((BYTE)expr->v.number);
        }
      else
        {
          ins_byte (F_STRING);
          ins_short ((short)expr->v.number);
        }
      break;
    case NODE_REAL:
      end_pushes ();
      ins_byte (F_REAL);
      ins_real (expr->v.real);
      break;
    case NODE_NUMBER:
      write_long_number (expr->v.number);
      break;
    case NODE_LAND_LOR:
      i_generate_node (expr->l.expr);
      i_generate_forward_branch ((BYTE)expr->v.number);
      i_generate_node (expr->r.expr);
      if (expr->l.expr->kind == NODE_BRANCH_LINK)
        {
          i_update_forward_branch_links ((BYTE)expr->v.number, expr->l.expr);
        }
      else
        i_update_forward_branch ();
      break;
    case NODE_BRANCH_LINK:
      i_generate_node (expr->l.expr);
      end_pushes ();
      ins_byte (0);
      expr->v.number = CURRENT_PROGRAM_SIZE;
      ins_short (0);
      i_generate_node (expr->r.expr);
      break;
    case NODE_CALL_2:
      generate_expr_list (expr->r.expr);
      end_pushes ();
      ins_byte ((BYTE)expr->v.number);
      ins_byte ((BYTE)(expr->l.number >> 16));
      ins_short ((short)(expr->l.number & 0xffff));
      ins_byte ((BYTE)(expr->r.expr ? expr->r.expr->kind : 0));
      break;
    case NODE_CALL_1:
      generate_expr_list (expr->r.expr);
      end_pushes ();
      ins_byte ((BYTE)expr->v.number);
      ins_short ((short)expr->l.number);
      ins_byte ((BYTE)(expr->r.expr ? expr->r.expr->kind : 0));
      break;
    case NODE_CALL:
      generate_expr_list (expr->r.expr);
      end_pushes ();
      ins_byte ((BYTE)expr->v.number);
      ins_short ((short)expr->l.number);
      break;
    case NODE_TWO_VALUES:
      i_generate_node (expr->l.expr);
      i_generate_node (expr->r.expr);
      break;
    case NODE_CONTROL_JUMP:
      {
        int kind = (int)expr->v.number;
        end_pushes ();
        ins_byte (F_BRANCH);
        expr->v.expr = branch_list[kind];
        expr->l.number = CURRENT_PROGRAM_SIZE;
        ins_short (0);
        branch_list[kind] = expr;
        break;
      }
    case NODE_PARAMETER:
      {
        int which = (int)expr->v.number + current_num_values;
        if (try_to_push (PUSH_LOCAL, which))
          break;
        ins_byte (F_LOCAL);
        ins_byte ((BYTE)which);
        break;
      }
    case NODE_PARAMETER_LVALUE:
      end_pushes ();
      ins_byte (F_LOCAL_LVALUE);
      ins_byte ((BYTE)(expr->v.number + current_num_values));
      break;
    case NODE_IF:
      i_generate_if_branch (expr->v.expr, 0);
      i_generate_node (expr->l.expr);
      if (expr->r.expr)
        {
          i_generate_else ();
          i_generate_node (expr->r.expr);
        }
      i_update_forward_branch ();
      break;
    case NODE_LOOP:
      i_generate_loop (expr->type, expr->v.expr, expr->l.expr, expr->r.expr);
      break;
    case NODE_FOREACH:
      {
        int tmp = 0;

        i_generate_node (expr->v.expr);
        end_pushes ();
        ins_byte (F_FOREACH);
        if (expr->l.expr->v.number == F_GLOBAL_LVALUE)
          tmp |= 1;
        if (expr->r.expr)
          {
            tmp |= 4;
            if (expr->r.expr->v.number == F_GLOBAL_LVALUE)
              tmp |= 2;
          }
        ins_byte ((BYTE)tmp);
        ins_byte ((BYTE)expr->l.expr->l.number);
        if (expr->r.expr)
          ins_byte ((BYTE)expr->r.expr->l.number);
      }
      break;
    case NODE_CASE_NUMBER:
    case NODE_CASE_STRING:
      if (expr->v.expr)
        {
          parse_node_t *other = expr->v.expr;
          expr->v.number = 1;
          other->l.expr = expr->l.expr;
          other->v.number = CURRENT_PROGRAM_SIZE;
          expr->l.expr = other;
        }
      else
        {
          expr->v.number = CURRENT_PROGRAM_SIZE;
        }
      end_pushes ();
      break;
    case NODE_DEFAULT:
      expr->v.number = CURRENT_PROGRAM_SIZE;
      end_pushes ();
      break;
    case NODE_SWITCH_STRINGS:
    case NODE_SWITCH_NUMBERS:
    case NODE_SWITCH_DIRECT:
    case NODE_SWITCH_RANGES:
      {
        int addr, last_break;
        parse_node_t *sub = expr->l.expr;
        parse_node_t *save_switch_breaks = branch_list[CJ_BREAK_SWITCH];

        i_generate_node (sub);
        branch_list[CJ_BREAK_SWITCH] = 0;
        end_pushes ();
        ins_byte (F_SWITCH);
        ins_byte (0xff);	/* kind of table */
        addr = (int)CURRENT_PROGRAM_SIZE;
        ins_short (0);		/* address of table */
        ins_short (0);		/* end of table */
        ins_short (0);		/* default address */
        i_generate_node (expr->r.expr);
        if (expr->v.expr && expr->v.expr->kind == NODE_DEFAULT)
          {
            upd_short (addr + 4, (short)expr->v.expr->v.number);
            expr->v.expr = expr->v.expr->l.expr;
          }
        else
          {
            upd_short (addr + 4, (short)CURRENT_PROGRAM_SIZE);
          }
        /* just in case the last case doesn't have a break */
        end_pushes ();
        ins_byte (F_BRANCH);
        last_break = (int)CURRENT_PROGRAM_SIZE;
        ins_short (0);
        /* build table */
        upd_short (addr, (short)CURRENT_PROGRAM_SIZE);
        if (expr->kind == NODE_SWITCH_STRINGS)
          {
            short sw = (short) (addr - 2);
            add_to_mem_block (A_PATCH, (char *) &sw, sizeof sw);
          }
        if (expr->kind == NODE_SWITCH_DIRECT)
          {
            parse_node_t *pn = expr->v.expr;
            while (pn)
              {
                ins_short ((short) pn->v.number);
                pn = pn->l.expr;
              }
            ins_int ((int)expr->v.expr->r.number);
            mem_block[current_block].block[addr - 1] = (char)0xfe;
          }
        else
          {
            int table_size = 0;
            int power_of_two = 1;
            int i = 0;
            parse_node_t *pn = expr->v.expr;

            while (pn)
              {
                if (expr->kind == NODE_SWITCH_STRINGS)
                  {
                    if (pn->r.number)
                      {
                        ins_intptr ((intptr_t)PROG_STRING (pn->r.number));
                      }
                    else
                      ins_intptr ((intptr_t) 0);
                  }
                else
                  ins_intptr ((intptr_t) pn->r.expr);
                ins_short ((short) pn->v.number);
                pn = pn->l.expr;
                table_size += 1;
              }
            while ((power_of_two << 1) <= table_size)
              {
                power_of_two <<= 1;
                i++;
              }
            if (expr->kind != NODE_SWITCH_STRINGS)
              mem_block[current_block].block[addr - 1] = (char) (0xf0 + i);
            else
              mem_block[current_block].block[addr - 1] =
                (char) (i * 0x10 + 0x0f);
          }
        i_update_branch_list (branch_list[CJ_BREAK_SWITCH]);
        branch_list[CJ_BREAK_SWITCH] = save_switch_breaks;
        upd_short (last_break, (short)(CURRENT_PROGRAM_SIZE - last_break));
        upd_short (addr + 2, (short)CURRENT_PROGRAM_SIZE);
        break;
      }
    case NODE_CATCH:
      {
        int addr;

        end_pushes ();
        ins_byte (F_CATCH);
        addr = (int)CURRENT_PROGRAM_SIZE;
        ins_short (0);
        i_generate_node (expr->r.expr);
        ins_byte (F_END_CATCH);
        upd_short (addr, (short)(CURRENT_PROGRAM_SIZE - addr));
        break;
      }
    case NODE_TIME_EXPRESSION:
      {
        end_pushes ();
        ins_byte (F_TIME_EXPRESSION);
        i_generate_node (expr->r.expr);
        ins_byte (F_END_TIME_EXPRESSION);
        break;
      }
    case NODE_LVALUE_EFUN:
      i_generate_node (expr->l.expr);
      generate_lvalue_list (expr->r.expr);
      break;
    case NODE_FUNCTION_CONSTRUCTOR:
      if (expr->r.expr)
        {
          generate_expr_list (expr->r.expr);
          end_pushes ();
          ins_byte (F_AGGREGATE);
          ins_short ((short)expr->r.expr->kind);
        }
      else
        {
          end_pushes ();
          ins_byte (F_CONST0);
        }
      end_pushes ();
      ins_byte (F_FUNCTION_CONSTRUCTOR);
      ins_byte ((BYTE)(expr->v.number & 0xff));

      switch (expr->v.number & 0xff)
        {
        case FP_SIMUL:
        case FP_LOCAL:
          ins_short ((short)(expr->v.number >> 8));
          break;
        case FP_EFUN:
          ins_short ((short)predefs[expr->v.number >> 8].token);
          break;
        case FP_FUNCTIONAL:
        case FP_FUNCTIONAL | FP_NOT_BINDABLE:
          {
            int addr, save_current_num_values = current_num_values;
            ins_byte ((BYTE)(expr->v.number >> 8));
            addr = (int)CURRENT_PROGRAM_SIZE;
            ins_short (0);
            current_num_values = expr->r.expr ? expr->r.expr->kind : 0;
            i_generate_node (expr->l.expr);
            current_num_values = save_current_num_values;
            end_pushes ();
            ins_byte (F_RETURN);
            upd_short (addr, (short)(CURRENT_PROGRAM_SIZE - addr - 2));
            break;
          }
        }
      break;
    case NODE_ANON_FUNC:
      {
        int addr;
        int save_fd = foreach_depth;

        foreach_depth = 0;
        end_pushes ();
        ins_byte (F_FUNCTION_CONSTRUCTOR);
        if (expr->v.number & 0x10000)
          ins_byte (FP_ANONYMOUS | FP_NOT_BINDABLE);
        else
          ins_byte (FP_ANONYMOUS);
        ins_byte ((BYTE)(expr->v.number & 0xff));
        ins_byte ((BYTE)expr->l.number);
        addr = (int)CURRENT_PROGRAM_SIZE;
        ins_short (0);
        i_generate_node (expr->r.expr);
        upd_short (addr, (short)(CURRENT_PROGRAM_SIZE - addr - 2));
        foreach_depth = save_fd;
        break;
      }
    case NODE_EFUN:
      {
        int novalue_used = expr->v.number & NOVALUE_USED_FLAG;
        int f = expr->v.number & ~NOVALUE_USED_FLAG;

        generate_expr_list (expr->r.expr);
        end_pushes ();
        if (f < ONEARG_MAX)
          {
            ins_byte ((BYTE)f);
          }
        else
          {
            /* max_arg == -1 must use F_EFUNV so that varargs expansion works */
            if (expr->l.number < 4 && instrs[f].max_arg != -1)
              ins_byte (F_EFUN0 + (BYTE)expr->l.number);
            else
              {
                ins_byte (F_EFUNV);
                ins_byte ((BYTE)expr->l.number);
              }
            ins_byte ((BYTE)(f - ONEARG_MAX));
          }
        if (novalue_used)
          {
            /* the value of a void efun was used.  Put in a zero. */
            ins_byte (F_CONST0);
          }
        break;
    default:
        fatal ("Unknown node %i in i_generate_node.\n", expr->kind);
      }
    }
}

static void
i_generate_loop (int test_first, parse_node_t * block,
                 parse_node_t * inc, parse_node_t * test)
{
  parse_node_t *save_breaks = branch_list[CJ_BREAK];
  parse_node_t *save_continues = branch_list[CJ_CONTINUE];
  int forever = node_always_true (test);
  int pos;

  if (test_first == 2)
    foreach_depth++;
  branch_list[CJ_BREAK] = branch_list[CJ_CONTINUE] = 0;
  end_pushes ();
  if (!forever && test_first)
    i_generate_forward_branch (F_BRANCH);
  pos = (int)CURRENT_PROGRAM_SIZE;
  i_generate_node (block);
  i_update_branch_list (branch_list[CJ_CONTINUE]);
  if (inc)
    i_generate_node (inc);
  if (!forever && test_first)
    i_update_forward_branch ();
  if (test->v.number == F_LOOP_COND_LOCAL ||
      test->v.number == F_LOOP_COND_NUMBER ||
      test->v.number == F_NEXT_FOREACH)
    {
      i_generate_node (test);
      ins_short ((short)(CURRENT_PROGRAM_SIZE - pos));
    }
  else
    i_branch_backwards ((BYTE)generate_conditional_branch (test), pos);
  i_update_branch_list (branch_list[CJ_BREAK]);
  branch_list[CJ_BREAK] = save_breaks;
  branch_list[CJ_CONTINUE] = save_continues;
  if (test_first == 2)
    foreach_depth--;
}

static void
i_generate_if_branch (parse_node_t * node, int invert)
{
  int generate_both = 0;
  int branch = (invert ? F_BRANCH_WHEN_NON_ZERO : F_BRANCH_WHEN_ZERO);

  switch (node->kind)
    {
    case NODE_UNARY_OP:
      if (node->v.number == F_NOT)
        {
          i_generate_if_branch (node->r.expr, !invert);
          return;
        }
      break;
    case NODE_BINARY_OP:
      switch (node->v.number)
        {
        case F_EQ:
          generate_both = 1;
          branch = (invert ? F_BRANCH_EQ : F_BRANCH_NE);
          break;
        case F_GE:
          if (invert)
            {
              generate_both = 1;
              branch = F_BRANCH_GE;
            }
          break;
        case F_LE:
          if (invert)
            {
              generate_both = 1;
              branch = F_BRANCH_LE;
            }
          break;
        case F_LT:
          if (!invert)
            {
              generate_both = 1;
              branch = F_BRANCH_GE;
            }
          break;
        case F_GT:
          if (!invert)
            {
              generate_both = 1;
              branch = F_BRANCH_LE;
            }
          break;
        case F_NE:
          generate_both = 1;
          branch = (invert ? F_BRANCH_NE : F_BRANCH_EQ);
          break;
        }
    }
  if (generate_both)
    {
      i_generate_node (node->l.expr);
      i_generate_node (node->r.expr);
    }
  else
    {
      i_generate_node (node);
    }
  i_generate_forward_branch ((BYTE)branch);
}

void
i_generate_inherited_init_call (int index, short f)
{
  end_pushes ();
  ins_byte (F_CALL_INHERITED);
  ins_byte ((BYTE)index);
  ins_short (f);
  ins_byte (0);
  ins_byte (F_POP_VALUE);
}

void
i_generate___INIT ()
{
  add_to_mem_block (A_PROGRAM, (char *) mem_block[A_INITIALIZER].block,
                    mem_block[A_INITIALIZER].current_size);
  prog_code = mem_block[A_PROGRAM].block + mem_block[A_PROGRAM].current_size;
}

void
i_generate_forward_branch (BYTE b)
{
  end_pushes ();
  ins_byte (b);
  ins_short ((short)current_forward_branch);
  current_forward_branch = CURRENT_PROGRAM_SIZE - 2;
}

void
i_update_forward_branch ()
{
  int i = read_short (current_forward_branch);

  end_pushes ();
  upd_short (current_forward_branch, (short)(CURRENT_PROGRAM_SIZE - current_forward_branch));
  current_forward_branch = i;
}

/**
 * @brief Update a list of forward branch links.
 * This function updates a list of forward branch links
 * generated by i_generate_forward_branch.
 * @param kind The kind of branch to update the links to.
 * @param link_start The start of the list of links.
 */
void i_update_forward_branch_links (BYTE kind, parse_node_t* link_start) {
  ptrdiff_t offset;

  end_pushes ();
  offset = read_short (current_forward_branch);
  upd_short (current_forward_branch,
             (short)(CURRENT_PROGRAM_SIZE - current_forward_branch));
  current_forward_branch = offset;
  do
    {
      offset = link_start->v.number;
      upd_byte (offset - 1, kind);
      upd_short (offset, (short)(CURRENT_PROGRAM_SIZE - offset));
      link_start = link_start->l.expr;
    }
  while (link_start->kind == NODE_BRANCH_LINK);
}

/**
 * @brief Generate a backwards branch.
 * This function generates a backwards branch to the given address.
 * @param kind The kind of branch to generate.
 * @param addr The address to branch to.
 */
void i_branch_backwards (BYTE kind, ptrdiff_t addr) {
  end_pushes ();
  if (kind)
    {
      if (kind != F_WHILE_DEC)
        ins_byte (kind);
      ins_short ((short)(CURRENT_PROGRAM_SIZE - addr));
    }
}

static void
i_update_branch_list (parse_node_t * bl)
{
  size_t current_size;

  end_pushes ();
  current_size = CURRENT_PROGRAM_SIZE;

  while (bl)
    {
      upd_short (bl->l.number, (short)(current_size - bl->l.number));
      bl = bl->v.expr;
    }
}

void
i_generate_else ()
{
  /* set up a new branch to after the end of the if */
  end_pushes ();
  ins_byte (F_BRANCH);
  /* save the old saved value here */
  ins_short (read_short (current_forward_branch));
  /* update the old branch to point to this point */
  upd_short (current_forward_branch,
             (short)(CURRENT_PROGRAM_SIZE - current_forward_branch));
  /* point current_forward_branch at the new branch we made */
  current_forward_branch = CURRENT_PROGRAM_SIZE - 2;
}

void
i_initialize_parser ()
{
  foreach_depth = 0;
  branch_list[CJ_BREAK] = 0;
  branch_list[CJ_BREAK_SWITCH] = 0;
  branch_list[CJ_CONTINUE] = 0;

  current_forward_branch = 0;

  current_block = A_PROGRAM;
  prog_code = mem_block[A_PROGRAM].block;
  prog_code_max = mem_block[A_PROGRAM].block + mem_block[A_PROGRAM].max_size;

  line_being_generated = 0;
  last_size_generated = 0;
}

void
i_generate_final_program (int x)
{
  if (!x)
    {
      UPDATE_PROGRAM_SIZE;
/* This needs work
 * if (pragmas & PRAGMA_OPTIMIZE)
 *     optimize_icode(0, 0, 0);
 */
      save_file_info (current_file_id, current_line - current_line_saved);
      switch_to_line (-1);	/* generate line numbers for the end */
    }
}

/**
 * Currently, this procedure handles:
 * - jump threading
 */
void optimize_icode (char *start, char *p, char *end) {
  int instr;
  if (start == 0)
    {
      /* we don't optimize the initializer block right now b/c all the
       * stuff we do (jump threading, etc) can't occur there.
       */
      start = mem_block[A_PROGRAM].block;
      p = start;
      end = p + mem_block[A_PROGRAM].current_size;
      if (*p == 0)
        {
          /* no initializer jump */
          p += 3;
        }
    }
  while (p < end)
    {
      switch (instr = EXTRACT_UCHAR (p++))
        {
        case F_NUMBER:
        case F_CALL_INHERITED:
          p += 4;
          break;
        case F_REAL:
          p += 8; /* [NEOLITH-EXTENSION] always use double-precision */
          break;
        case F_SIMUL_EFUN:
        case F_CALL_FUNCTION_BY_ADDRESS:
          p += 3;
          break;
        case F_BRANCH:
        case F_BRANCH_WHEN_ZERO:
        case F_BRANCH_WHEN_NON_ZERO:
        case F_BBRANCH:
        case F_BBRANCH_WHEN_ZERO:
        case F_BBRANCH_WHEN_NON_ZERO:
          {
            char *tmp;
            short sarg;
            /* thread jumps */
            COPY_SHORT (&sarg, p);
            if (instr > F_BRANCH)
              tmp = p - sarg;
            else
              tmp = p + sarg;
            sarg = 0;
            while (1)
              {
                if (EXTRACT_UCHAR (tmp) == F_BRANCH)
                  {
                    COPY_SHORT (&sarg, tmp + 1);
                    tmp += sarg + 1;
                  }
                else if (EXTRACT_UCHAR (tmp) == F_BBRANCH)
                  {
                    COPY_SHORT (&sarg, tmp + 1);
                    tmp -= sarg - 1;
                  }
                else
                  break;
              }
            if (!sarg)
              {
                p += 2;
                break;
              }
            /* be careful; in the process of threading a forward jump
             * may have changed to a reverse one or vice versa
             */
            if (tmp > p)
              {
                if (instr > F_BRANCH)
                  {
                    p[-1] -= 3;	/* change to forward branch */
                  }
                sarg = (short)(tmp - p);
              }
            else
              {
                if (instr <= F_BRANCH)
                  {
                    p[-1] += 3;	/* change to backwards branch */
                  }
                sarg = (short)(p - tmp);
              }
            STORE_SHORT (p, sarg);
            break;
          }
#ifdef F_LOR
        case F_LOR:
        case F_LAND:
          {
            char *tmp;
            short sarg;
            /* thread jumps */
            COPY_SHORT (&sarg, p);
            tmp = p + sarg;
            sarg = 0;
            while (1)
              {
                if (EXTRACT_UCHAR (tmp) == F_BRANCH)
                  {
                    COPY_SHORT (&sarg, tmp + 1);
                    tmp += sarg + 1;
                  }
                else if (EXTRACT_UCHAR (tmp) == F_BBRANCH)
                  {
                    COPY_SHORT (&sarg, tmp + 1);
                    tmp -= sarg - 1;
                  }
                else
                  break;
              }
            if (!sarg)
              {
                p += 2;
                break;
              }
            /* be careful; in the process of threading a forward jump
             * may have changed to a reverse one or vice versa
             */
            if (tmp > p)
              {
                sarg = (short)(tmp - p);
              }
            else
              {
                p += 2;
                break;
              }
            STORE_SHORT (p, sarg);
            break;
          }
#endif
        case F_CATCH:
        case F_AGGREGATE:
        case F_AGGREGATE_ASSOC:
        case F_STRING:
#ifdef F_JUMP_WHEN_ZERO
        case F_JUMP_WHEN_ZERO:
        case F_JUMP_WHEN_NON_ZERO:
#endif
#ifdef F_JUMP
        case F_JUMP:
#endif
          p += 2;
          break;
        case F_GLOBAL_LVALUE:
        case F_GLOBAL:
        case F_SHORT_STRING:
        case F_LOOP_INCR:
        case F_WHILE_DEC:
        case F_LOCAL:
        case F_LOCAL_LVALUE:
        case F_SSCANF:
        case F_PARSE_COMMAND:
        case F_BYTE:
        case F_NBYTE:
          p++;
          break;
        case F_FUNCTION_CONSTRUCTOR:
          switch (EXTRACT_UCHAR (p++))
            {
            case FP_SIMUL:
            case FP_LOCAL:
              p += 2;
              break;
            case FP_FUNCTIONAL:
            case FP_FUNCTIONAL | FP_NOT_BINDABLE:
              p += 3;
              break;
            case FP_ANONYMOUS:
            case FP_ANONYMOUS | FP_NOT_BINDABLE:
              p += 4;
              break;
            case FP_EFUN:
              p += 2;
              break;
            }
          break;
        case F_SWITCH:
          {
            unsigned short stable, etable;
            p++;		/* table type */
            LOAD_SHORT (stable, p);
            LOAD_SHORT (etable, p);
            p += 2;		/* def */
            DEBUG_CHECK (stable < p - start || etable < p - start
                         || etable < stable,
                         "Error in switch table found while optimizing\n");
            /* recursively optimize the inside of the switch */
            optimize_icode (start, p, start + stable);
            p = start + etable;
            break;
          }
        case F_EFUN0:
        case F_EFUN1:
        case F_EFUN2:
        case F_EFUN3:
        case F_EFUNV:
          instr = EXTRACT_UCHAR (p++) + ONEARG_MAX;
          /* fall through */
        default:
          if ((instr >= BASE) &&
              (instrs[instr].min_arg != instrs[instr].max_arg))
            p++;
        }
    }
}
