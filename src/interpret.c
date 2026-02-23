#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "rc.h"
#include "comm.h"
#include "qsort.h"
#include "apply.h"
#include "frame.h"
#include "interpret.h"
#include "simul_efun.h"
#include "lpc/object.h"
#include "lpc/array.h"
#include "lpc/buffer.h"
#include "lpc/mapping.h"
#include "lpc/class.h"
#include "lpc/operator.h"
#include "lpc/include/function.h"
#include "lpc/include/origin.h"
#include "efuns/parse.h"
#include "efuns/sscanf.h"

#include "efuns_prototype.h"
#include "efuns_vector.h"

int num_varargs;
int caller_type;
program_t *current_prog;

static void do_loop_cond_number (void);
static void do_loop_cond_local (void);

/*
 * Inheritance:
 * An object X can inherit from another object Y. This is done with
 * the statement 'inherit "file";'
 * The inherit statement will clone a copy of that file, call reset
 * in it, and set a pointer to Y from X.
 * Y has to be removed from the linked list of all objects.
 * All variables declared by Y will be copied to X, so that X has access
 * to them.
 *
 * If Y isn't loaded when it is needed, X will be discarded, and Y will be
 * loaded separately. X will then be reloaded again.
 */

/*
 * These are the registers used at runtime.
 * The control stack saves registers to be restored when a function
 * will return. That means that control_stack[0] will have almost no
 * interesting values, as it will terminate execution.
 */
const char *pc;         /* Program pointer. */

int function_index_offset;	/* Needed for inheritance */
int variable_index_offset;	/* Needed for inheritance */

/*
 * Information about assignments of values:
 *
 * There are three types of l-values: Local variables, global variables
 * and array elements.
 *
 * The local variables are allocated on the stack together with the arguments.
 * the register 'frame_pointer' points to the first argument.
 *
 * The global variables must keep their values between executions, and
 * have space allocated at the creation of the object.
 *
 * Elements in arrays are similar to global variables. There is a reference
 * count to the whole array, that states when to deallocate the array.
 * The elements consists of 'svalue_t's, and will thus have to be freed
 * immediately when over written.
 */

/*
 * Get address to a valid global variable.
 */
#define find_value(num) (&current_object->variables[num])

void process_efun_callback (int narg, function_to_call_t * ftc, int f) {
  svalue_t *arg = sp - st_num_arg + 1 + narg;

  if (arg->type == T_FUNCTION)
    {
      ftc->f.fp = arg->u.fp;
      ftc->ob = 0;
      ftc->narg = st_num_arg - narg - 1;
      ftc->args = arg + 1;
    }
  else
    {
      ftc->f.str = arg->u.string;
      if (st_num_arg < narg + 2)
        {
          ftc->ob = current_object;
          ftc->narg = 0;
        }
      else
        {
          if ((arg + 1)->type == T_OBJECT)
            {
              ftc->ob = (arg + 1)->u.ob;
            }
          else if ((arg + 1)->type == T_STRING)
            {
              if (!(ftc->ob = find_or_load_object ((arg + 1)->u.string)) ||
                  !object_visible (ftc->ob))
                bad_argument (arg + 1, T_STRING | T_OBJECT, 3, f);
            }
          else
            bad_argument (arg + 1, T_STRING | T_OBJECT, 3, f);

          ftc->narg = st_num_arg - narg - 2;
          ftc->args = arg + 2;

          if (ftc->ob->flags & O_DESTRUCTED)
            bad_argument (arg + 1, T_STRING | T_OBJECT, 3, f);
        }
    }
}

svalue_t* call_efun_callback (function_to_call_t * ftc, int n) {
  svalue_t *v;

  if (ftc->narg)
    push_some_svalues (ftc->args, ftc->narg);

  if (ftc->ob)
    {
      if (ftc->ob->flags & O_DESTRUCTED)
        error ("*Object destructed during efun callback.");
      v = apply (ftc->f.str, ftc->ob, n + ftc->narg, ORIGIN_EFUN);
    }
  else
    v = call_function_pointer (ftc->f.fp, n + ftc->narg);

  return v;
}

static svalue_t global_lvalue_byte = { .type = T_LVALUE_BYTE };

/**
 * Compute the address of an array element.
 * Used by the F_INDEX_LVALUE and F_RINDEX_LVALUE opcodes.
 * The index is on the stack, as is the lvalue to be indexed.
 */
static void push_indexed_lvalue (int reverse) {
  int64_t ind;
  svalue_t *lv;

  if (sp->type == T_LVALUE)
    {
      lv = sp->u.lvalue;
      if (!reverse && lv->type == T_MAPPING)
        {
          sp--;
          if (!(lv = find_for_insert (lv->u.map, sp, 0)))
            mapping_too_large ();
          free_svalue (sp, "push_indexed_lvalue: 1");
          sp->type = T_LVALUE;
          sp->u.lvalue = lv;
          return;
        }

      if (!((--sp)->type == T_NUMBER))
        error ("*Illegal type of index");

      ind = sp->u.number;

      switch (lv->type)
        {
        case T_STRING:
          {
            size_t len = SVALUE_STRLEN (lv);

            if (reverse)
              ind = len - ind;
            if (ind >= (int64_t)len || ind < 0)
              error ("*Index out of bounds in string index lvalue.");
            unlink_string_svalue (lv);
            sp->type = T_LVALUE;
            sp->u.lvalue = &global_lvalue_byte;
            global_lvalue_byte.u.lvalue_byte = (unsigned char *) &lv->u.string[ind];
            break;
          }

        case T_BUFFER:
          {
            if (reverse)
              ind = lv->u.buf->size - ind;
            if (ind >= (int)lv->u.buf->size || ind < 0)
              error ("*Buffer index out of bounds.");
            sp->type = T_LVALUE;
            sp->u.lvalue = &global_lvalue_byte;
            global_lvalue_byte.u.lvalue_byte = &lv->u.buf->item[ind];
            break;
          }

        case T_ARRAY:
          {
            if (reverse)
              ind = lv->u.arr->size - ind;
            if (ind >= lv->u.arr->size || ind < 0)
              error ("*Array index out of bounds.");
            sp->type = T_LVALUE;
            sp->u.lvalue = lv->u.arr->item + ind;
            break;
          }

        default:
          if (lv->type == T_NUMBER && !lv->u.number)
            error ("*Value being indexed is zero.");
          error ("*Cannot index value of type '%s'.", type_name (lv->type));
        }
    }
  else
    {
      /* It is now coming from:
       *
       *   (x <assign_type> y)[index]... = rhs;
       * 
       * where x is a valid lvalue.
       * Hence the reference to sp is at least 2 :)
       */
      if (!reverse && (sp->type == T_MAPPING))
        {
          if (!(lv = find_for_insert (sp->u.map, sp - 1, 0)))
            mapping_too_large ();
          sp->u.map->ref--;
          free_svalue (--sp, "push_indexed_lvalue: 2");
          sp->type = T_LVALUE;
          sp->u.lvalue = lv;
          return;
        }

      if (!((sp - 1)->type == T_NUMBER))
        error ("*Illegal type of index.");

      ind = (sp - 1)->u.number;

      switch (sp->type)
        {
        case T_STRING:
          {
            error ("*Illegal to make char lvalue from assigned string.");
            break;
          }

        case T_BUFFER:
          {
            if (reverse)
              ind = sp->u.buf->size - ind;
            if (ind >= (int)sp->u.buf->size || ind < 0)
              error ("*Buffer index out of bounds.");
            sp->u.buf->ref--;
            (--sp)->type = T_LVALUE;
            sp->u.lvalue = &global_lvalue_byte;
            global_lvalue_byte.u.lvalue_byte = (sp + 1)->u.buf->item + ind;
            break;
          }

        case T_ARRAY:
          {
            if (reverse)
              ind = sp->u.arr->size - ind;
            if (ind >= sp->u.arr->size || ind < 0)
              error ("*Array index out of bounds.");
            sp->u.arr->ref--;
            (--sp)->type = T_LVALUE;
            sp->u.lvalue = (sp + 1)->u.arr->item + ind;
            break;
          }

        default:
          if (sp->type == T_NUMBER && !sp->u.number)
            error ("*Value being indexed is zero.");
          error ("*Cannot index value of type '%s'.", type_name (sp->type));
        }
    }
}

static struct lvalue_range {
  int ind1, ind2, size;
  svalue_t *owner;
} global_lvalue_range;

static svalue_t global_lvalue_range_sv = { .type = T_LVALUE_RANGE };

static void push_lvalue_range (int code) {
  int ind1, ind2, size = 0;
  svalue_t *lv;

  if (sp->type == T_LVALUE)
    {
      switch ((lv = global_lvalue_range.owner = sp->u.lvalue)->type)
        {
        case T_ARRAY:
          size = lv->u.arr->size;
          break;
        case T_STRING:
          {
            size = (int)SVALUE_STRLEN (lv);
            unlink_string_svalue (lv);
            break;
          }
        case T_BUFFER:
          size = lv->u.buf->size;
          break;
        default:
          error ("*Range lvalue on illegal type.");
          IF_DEBUG (size = 0);
        }
    }
  else
    error ("*Range lvalue on illegal type.");

  if (!((--sp)->type == T_NUMBER))
    error ("*Illegal 2nd index type to range lvalue.");

  ind2 = (code & 0x01) ? (size - (int)sp->u.number) : (int)sp->u.number;
  if (++ind2 < 0 || (ind2 > size))
    error ("*The 2nd index to range lvalue must be >= -1 and < sizeof(indexed value)");

  if (!((--sp)->type == T_NUMBER))
    error ("*Illegal 1st index type to range lvalue");

  ind1 = (code & 0x10) ? (size - (int)sp->u.number) : (int)sp->u.number;

  if (ind1 < 0 || ind1 > size)
    error ("*The 1st index to range lvalue must be >= 0 and <= sizeof(indexed value)");

  global_lvalue_range.ind1 = ind1;
  global_lvalue_range.ind2 = ind2;
  global_lvalue_range.size = size;
  sp->type = T_LVALUE;
  sp->u.lvalue = &global_lvalue_range_sv;
}

static void copy_lvalue_range (svalue_t * from) {
  int ind1, ind2, size, fsize;
  svalue_t *owner;

  ind1 = global_lvalue_range.ind1;
  ind2 = global_lvalue_range.ind2;
  size = global_lvalue_range.size;
  owner = global_lvalue_range.owner;

  switch (owner->type)
    {
    case T_ARRAY:
      {
        array_t *fv, *dv;
        svalue_t *fptr, *dptr;
        if (from->type != T_ARRAY)
          error ("*Illegal rhs to array range lvalue.");

        fv = from->u.arr;
        fptr = fv->item;

        if ((fsize = fv->size) == ind2 - ind1)
          {
            dptr = (owner->u.arr)->item + ind1;

            if (fv->ref == 1)
              {
                /* Transfer the svalues */
                while (fsize--)
                  {
                    free_svalue (dptr, "copy_lvalue_range : 1");
                    *dptr++ = *fptr++;
                  }
                free_empty_array (fv);
              }
            else
              {
                while (fsize--)
                  assign_svalue (dptr++, fptr++);
                fv->ref--;
              }
          }
        else
          {
            array_t *old_dv = owner->u.arr;
            svalue_t *old_dptr = old_dv->item;

            /* Need to reallocate the array */
            dv = allocate_empty_array (size - ind2 + ind1 + fsize);
            dptr = dv->item;

            /* ind1 can range from 0 to sizeof(old_dv) */
            while (ind1--)
              assign_svalue_no_free (dptr++, old_dptr++);

            if (fv->ref == 1)
              {
                while (fsize--)
                  *dptr++ = *fptr++;
                free_empty_array (fv);
              }
            else
              {
                while (fsize--)
                  assign_svalue_no_free (dptr++, fptr++);
                fv->ref--;
              }

            /* ind2 can range from 0 to sizeof(old_dv) */
            old_dptr = old_dv->item + ind2;
            size -= ind2;

            while (size--)
              assign_svalue_no_free (dptr++, old_dptr++);
            free_array (old_dv);

            owner->u.arr = dv;
          }
        break;
      }

    case T_STRING:
      {
        if (from->type != T_STRING)
          error ("*Illegal rhs to string range lvalue.");

        fsize = (int)SVALUE_STRLEN (from);
        if (fsize == ind2 - ind1)
          {
            /* since fsize >= 0, ind2 - ind1 <= strlen(orig string) */
            /* because both of them can only range from 0 to len */

            strncpy (owner->u.string + ind1, from->u.string, fsize);
          }
        else
          {
            char *tmp, *dstr = owner->u.string;

            owner->u.string = tmp = new_string (size - ind2 + ind1 + fsize, "copy_lvalue_range");
            if (ind1 >= 1)
              {
                strncpy (tmp, dstr, ind1);
                tmp += ind1;
              }
            strcpy (tmp, from->u.string);
            tmp += fsize;

            size -= ind2;
            if (size >= 1)
              {
                strncpy (tmp, dstr + ind2, size);
                *(tmp + size) = 0;
              }
            FREE_MSTR (dstr);
          }
        free_string_svalue (from);
        break;
      }

    case T_BUFFER:
      {
        if (from->type != T_BUFFER)
          error ("*Illegal rhs to buffer range lvalue.");

        if ((fsize = from->u.buf->size) == ind2 - ind1)
          {
            memcpy ((owner->u.buf)->item + ind1, from->u.buf->item, fsize);
          }
        else
          {
            buffer_t *b;
            unsigned char *old_item = (owner->u.buf)->item;
            unsigned char *new_item;

            b = allocate_buffer (size - ind2 + ind1 + fsize);
            new_item = b->item;
            if (ind1 >= 1)
              {
                memcpy (b->item, old_item, ind1);
                new_item += ind1;
              }
            memcpy (new_item, from->u.buf, fsize);
            new_item += fsize;

            if ((size -= ind2) >= 1)
              memcpy (new_item, old_item + ind2, size);
            free_buffer (owner->u.buf);
            owner->u.buf = b;
          }
        free_buffer (from->u.buf);
        break;
      }
    }
}

static void assign_lvalue_range (svalue_t * from) {
  int ind1, ind2, size, fsize;
  svalue_t *owner;

  ind1 = global_lvalue_range.ind1;
  ind2 = global_lvalue_range.ind2;
  size = global_lvalue_range.size;
  owner = global_lvalue_range.owner;

  switch (owner->type)
    {
    case T_ARRAY:
      {
        array_t *fv, *dv;
        svalue_t *fptr, *dptr;
        if (from->type != T_ARRAY)
          error ("*Illegal rhs to array range lvalue.");

        fv = from->u.arr;
        fptr = fv->item;

        if ((fsize = fv->size) == ind2 - ind1)
          {
            dptr = (owner->u.arr)->item + ind1;
            while (fsize--)
              assign_svalue (dptr++, fptr++);
          }
        else
          {
            array_t *old_dv = owner->u.arr;
            svalue_t *old_dptr = old_dv->item;

            /* Need to reallocate the array */
            dv = allocate_empty_array (size - ind2 + ind1 + fsize);
            dptr = dv->item;

            /* ind1 can range from 0 to sizeof(old_dv) */
            while (ind1--)
              assign_svalue_no_free (dptr++, old_dptr++);

            while (fsize--)
              assign_svalue_no_free (dptr++, fptr++);

            /* ind2 can range from 0 to sizeof(old_dv) */
            old_dptr = old_dv->item + ind2;
            size -= ind2;

            while (size--)
              assign_svalue_no_free (dptr++, old_dptr++);
            free_array (old_dv);

            owner->u.arr = dv;
          }
        break;
      }

    case T_STRING:
      {
        if (from->type != T_STRING)
          error ("*Illegal rhs to string range lvalue.");

        fsize = (int)SVALUE_STRLEN (from);
        if (fsize == ind2 - ind1)
          {
            /* since fsize >= 0, ind2 - ind1 <= strlen(orig string) */
            /* because both of them can only range from 0 to len */

            strncpy (owner->u.string + ind1, from->u.string, fsize);
          }
        else
          {
            char *tmp, *dstr = owner->u.string;

            owner->u.string = tmp =
              new_string (size - ind2 + ind1 + fsize, "assign_lvalue_range");
            if (ind1 >= 1)
              {
                strncpy (tmp, dstr, ind1);
                tmp += ind1;
              }
            strcpy (tmp, from->u.string);
            tmp += fsize;

            size -= ind2;
            if (size >= 1)
              {
                strncpy (tmp, dstr + ind2, size);
                *(tmp + size) = 0;
              }
            FREE_MSTR (dstr);
          }
        break;
      }

    case T_BUFFER:
      {
        if (from->type != T_BUFFER)
          error ("*Illegal rhs to buffer range lvalue.");

        if ((fsize = from->u.buf->size) == ind2 - ind1)
          {
            memcpy ((owner->u.buf)->item + ind1, from->u.buf->item, fsize);
          }
        else
          {
            buffer_t *b;
            unsigned char *old_item = (owner->u.buf)->item;
            unsigned char *new_item;

            b = allocate_buffer (size - ind2 + ind1 + fsize);
            new_item = b->item;
            if (ind1 >= 1)
              {
                memcpy (b->item, old_item, ind1);
                new_item += ind1;
              }
            memcpy (new_item, from->u.buf, fsize);
            new_item += fsize;

            if ((size -= ind2) >= 1)
              memcpy (new_item, old_item + ind2, size);
            free_buffer (owner->u.buf);
            owner->u.buf = b;
          }
        break;
      }
    }
}

/* do_loop_cond() coded by John Garnett, 1993/06/01
   
   Optimizes these four cases (with 'int i'):
   
   1) for (expr0; i < integer_variable; expr2) statement;
   2) for (expr0; i < integer_constant; expr2) statement;
   3) while (i < integer_variable) statement;
   4) while (i < integer_constant) statement;
   */
static void do_loop_cond_local () {
  svalue_t *s1, *s2;
  int i;

  s1 = fp + EXTRACT_UCHAR (pc++);	/* a from (a < b) */
  s2 = fp + EXTRACT_UCHAR (pc++);
  switch (s1->type | s2->type)
    {
    case T_NUMBER:
      i = s1->u.number < s2->u.number;
      break;
    case T_REAL:
      i = s1->u.real < s2->u.real;
      break;
    case T_STRING:
      i = (strcmp (s1->u.string, s2->u.string) < 0);
      break;
    case T_NUMBER | T_REAL:
      if (s1->type == T_NUMBER)
        i = s1->u.number < s2->u.real;
      else
        i = s1->u.real < s2->u.number;
      break;
    default:
      if (s1->type == T_OBJECT && (s1->u.ob->flags & O_DESTRUCTED))
        {
          free_object (s1->u.ob, "do_loop_cond:1");
          *s1 = const0;
        }
      if (s2->type == T_OBJECT && (s2->u.ob->flags & O_DESTRUCTED))
        {
          free_object (s2->u.ob, "do_loop_cond:2");
          *s2 = const0;
        }
      if (s1->type == T_NUMBER && s2->type == T_NUMBER)
        {
          i = s1->u.number < s2->u.number;
          break;
        }
      switch (s1->type)
        {
        case T_NUMBER:
        case T_REAL:
          error ("*2nd argument to < is not numeric when the 1st is.");
        case T_STRING:
          error ("*2nd argument to < is not string when the 1st is.");
        default:
          error ("*Bad 1st argument to <.");
        }
      i = 0;
    }
  if (i)
    {
      unsigned short offset;

      COPY_SHORT (&offset, pc);
      pc -= offset;
    }
  else
    pc += 2;
}

static void do_loop_cond_number () {
  svalue_t *s1;
  int i;

  s1 = fp + EXTRACT_UCHAR (pc++);	/* a from (a < b) */
  LOAD_INT (i, pc);
  if (s1->type == T_NUMBER)
    {
      if (s1->u.number < i)
        {
          unsigned short offset;

          COPY_SHORT (&offset, pc);
          pc -= offset;
        }
      else
        pc += 2;
    }
  else if (s1->type == T_REAL)
    {
      if (s1->u.real < i)
        {
          unsigned short offset;

          COPY_SHORT (&offset, pc);
          pc -= offset;
        }
      else
        pc += 2;
    }
  else
    error ("*Right side of < is a number, left side is not.");
}

/**
 *  @brief Evaluate instructions at address \p p.
 *  All program offsets are relative to \p current_prog->program.
 *  ( \p current_prog must be setup before call of this function. )
 *
 *  There must not be destructed objects on the stack.
 *  The destruct_object() function will automatically remove all occurences.
 *  The effect is that all called efuns knows that they won't have destructed objects as
 *  arguments.
 */
void eval_instruction (const char *p) {

  int i, n;
  double real;
  svalue_t *lval;
  int instruction;
  unsigned short offset;
  static instr_t *instrs2 = instrs + ONEARG_MAX;

  /* Next F_RETURN at this level will return out of eval_instruction() */
  csp->framekind |= FRAME_EXTERNAL;
  pc = p; // current_prog->program
  while (1)
    {
      instruction = EXTRACT_UCHAR (pc++);
      if (!--eval_cost)
        {
          /* [NEOLITH-EXTENSION] allows eval_instruction without current_object */
          if (current_object)
            debug_message ("object /%s: eval_cost too big %d\n", current_object->name, CONFIG_INT (__MAX_EVAL_COST__));
          set_error_state (ES_MAX_EVAL_COST);

          eval_cost = CONFIG_INT (__MAX_EVAL_COST__);
          error ("*Too long evaluation. Execution aborted.");
        }
      /*
       * Execute current instruction. Note that all functions callable from
       * LPC must return a value. This does not apply to control
       * instructions, like F_JUMP.
       */
      switch (instruction)
        {
        case F_PUSH:		/* Push a number of things onto the stack */
          n = EXTRACT_UCHAR (pc++);
          while (n--)
            {
              i = EXTRACT_UCHAR (pc++);
              switch (i & PUSH_WHAT)
                {
                case PUSH_STRING:
                  push_shared_string (current_prog->strings[i & PUSH_MASK]);
                  break;
                case PUSH_LOCAL:
                  lval = fp + (i & PUSH_MASK);
                  if ((lval->type == T_OBJECT) && (lval->u.ob->flags & O_DESTRUCTED))
                    {
                      *++sp = const0;
                      assign_svalue (lval, &const0);
                    }
                  else
                    {
                      push_svalue (lval);
                    }
                  break;
                case PUSH_GLOBAL:
                  lval =
                    find_value ((int)((i & PUSH_MASK) + variable_index_offset));
                  if ((lval->type == T_OBJECT) && (lval->u.ob->flags & O_DESTRUCTED))
                    {
                      *++sp = const0;
                      assign_svalue (lval, &const0);
                    }
                  else
                    {
                      push_svalue (lval);
                    }
                  break;
                case PUSH_NUMBER:
                  push_number (i & PUSH_MASK);
                  break;
                }
            }
          break;
        case F_INC:
          lval = (sp--)->u.lvalue;
          switch (lval->type)
            {
            case T_NUMBER:
              lval->u.number++;
              break;
            case T_REAL:
              lval->u.real++;
              break;
            case T_LVALUE_BYTE:
              if (*global_lvalue_byte.u.lvalue_byte == (unsigned char) 255)
                error ("*Strings cannot contain 0 bytes.");
              ++*global_lvalue_byte.u.lvalue_byte;
              break;
            default:
              error ("*Increment (++) on non-numeric argument.");
            }
          break;
        case F_WHILE_DEC:
          {
            svalue_t *s;

            s = fp + EXTRACT_UCHAR (pc++);
            if (s->type == T_NUMBER)
              {
                i = (int)s->u.number--;
              }
            else if (s->type == T_REAL)
              {
                i = (int)s->u.real--;
              }
            else
              {
                i = 0;
                error ("*Decrement (--) on non-numeric argument.");
              }
            if (i)
              {
                COPY_SHORT (&offset, pc);
                pc -= offset;
              }
            else
              {
                pc += 2;
              }
          }
          break;
        case F_LOCAL_LVALUE:
          (++sp)->type = T_LVALUE;
          sp->u.lvalue = fp + EXTRACT_UCHAR (pc++);
          break;
        case F_NUMBER:
          LOAD_INT (i, pc);
          push_number (i);
          break;
        case F_LONG:
          {
            int64_t long_val;
            LOAD_LONG (long_val, pc);
            push_number (long_val);
          }
          break;
        case F_REAL:
          LOAD_FLOAT (real, pc);
          push_real (real);
          break;
        case F_BYTE:
          push_number (EXTRACT_UCHAR (pc++));
          break;
        case F_NBYTE:
          push_number (-((int) EXTRACT_UCHAR (pc++)));
          break;
#ifdef F_JUMP_WHEN_NON_ZERO
        case F_JUMP_WHEN_NON_ZERO:
          if ((i = (sp->type == T_NUMBER)) && (sp->u.number == 0))
            pc += 2;
          else
            {
              COPY_SHORT (&offset, pc);
              pc = current_prog->program + offset; // F_JUMP_WHEN_NON_ZERO
            }
          if (i)
            {
              sp--;		/* when sp is an integer svalue, its cheaper
                                 * to do this */
            }
          else
            {
              pop_stack ();
            }
          break;
#endif
        case F_BRANCH:		/* relative offset */
          COPY_SHORT (&offset, pc);
          pc += offset;
          break;
        case F_BBRANCH:	/* relative offset */
          COPY_SHORT (&offset, pc);
          pc -= offset;
          break;
        case F_BRANCH_NE:
          f_ne ();
          if ((sp--)->u.number)
            {
              COPY_SHORT (&offset, pc);
              pc += offset;
            }
          else
            pc += 2;
          break;
        case F_BRANCH_GE:
          f_ge ();
          if ((sp--)->u.number)
            {
              COPY_SHORT (&offset, pc);
              pc += offset;
            }
          else
            pc += 2;
          break;
        case F_BRANCH_LE:
          f_le ();
          if ((sp--)->u.number)
            {
              COPY_SHORT (&offset, pc);
              pc += offset;
            }
          else
            pc += 2;
          break;
        case F_BRANCH_EQ:
          f_eq ();
          if ((sp--)->u.number)
            {
              COPY_SHORT (&offset, pc);
              pc += offset;
            }
          else
            pc += 2;
          break;
        case F_BBRANCH_LT:
          f_lt ();
          if ((sp--)->u.number)
            {
              COPY_SHORT (&offset, pc);
              pc -= offset;
            }
          else
            pc += 2;
          break;
        case F_BRANCH_WHEN_ZERO:	/* relative offset */
          if (sp->type == T_NUMBER)
            {
              if (!((sp--)->u.number))
                {
                  COPY_SHORT (&offset, pc);
                  pc += offset;
                  break;
                }
            }
          else
            pop_stack ();
          pc += 2;		/* skip over the offset */
          break;
        case F_BRANCH_WHEN_NON_ZERO:	/* relative offset */
          if (sp->type == T_NUMBER)
            {
              if (!((sp--)->u.number))
                {
                  pc += 2;
                  break;
                }
            }
          else
            pop_stack ();
          COPY_SHORT (&offset, pc);
          pc += offset;
          break;
        case F_BBRANCH_WHEN_ZERO:	/* relative backwards offset */
          if (sp->type == T_NUMBER)
            {
              if (!((sp--)->u.number))
                {
                  COPY_SHORT (&offset, pc);
                  pc -= offset;
                  break;
                }
            }
          else
            pop_stack ();
          pc += 2;
          break;
        case F_BBRANCH_WHEN_NON_ZERO:	/* relative backwards offset */
          if (sp->type == T_NUMBER)
            {
              if (!((sp--)->u.number))
                {
                  pc += 2;
                  break;
                }
            }
          else
            pop_stack ();
          COPY_SHORT (&offset, pc);
          pc -= offset;
          break;
        case F_LOR:
          /* replaces F_DUP; F_BRANCH_WHEN_NON_ZERO; F_POP */
          if (sp->type == T_NUMBER)
            {
              if (!sp->u.number)
                {
                  pc += 2;
                  sp--;
                  break;
                }
            }
          COPY_SHORT (&offset, pc);
          pc += offset;
          break;
        case F_LAND:
          /* replaces F_DUP; F_BRANCH_WHEN_ZERO; F_POP */
          if (sp->type == T_NUMBER)
            {
              if (!sp->u.number)
                {
                  COPY_SHORT (&offset, pc);
                  pc += offset;
                  break;
                }
              sp--;
            }
          else
            pop_stack ();
          pc += 2;
          break;
        case F_LOOP_INCR:	/* this case must be just prior to
                                 * F_LOOP_COND */
          {
            svalue_t *s;

            s = fp + EXTRACT_UCHAR (pc++);
            if (s->type == T_NUMBER)
              {
                s->u.number++;
              }
            else if (s->type == T_REAL)
              {
                s->u.real++;
              }
            else
              {
                error ("*Increment (++) on non-numeric argument.");
              }
          }
          if (*pc == F_LOOP_COND_LOCAL)
            {
              pc++;
              do_loop_cond_local ();
            }
          else if (*pc == F_LOOP_COND_NUMBER)
            {
              pc++;
              do_loop_cond_number ();
            }
          break;
        case F_LOOP_COND_LOCAL:
          do_loop_cond_local ();
          break;
        case F_LOOP_COND_NUMBER:
          do_loop_cond_number ();
          break;
        case F_TRANSFER_LOCAL:
          {
            svalue_t *s;

            s = fp + EXTRACT_UCHAR (pc++);
            DEBUG_CHECK ((fp - s) >= csp->num_local_variables, "Tried to push non-existent local\n");
            if ((s->type == T_OBJECT) && (s->u.ob->flags & O_DESTRUCTED))
              {
                *++sp = const0;
                free_object (s->u.ob, "Transfer dested object");
                *s = const0;
                break;
              }
            *++sp = *s;

            /* The optimizer has asserted this won't be used again.  Make
             * it look like a number to avoid double frees. */
            s->type = T_NUMBER;
            break;
          }
        case F_LOCAL:
          {
            svalue_t *s;

            s = fp + EXTRACT_UCHAR (pc++);
            DEBUG_CHECK ((fp - s) >= csp->num_local_variables, "Tried to push non-existent local\n");

            /*
             * If variable points to a destructed object, replace it
             * with 0, otherwise, fetch value of variable.
             */
            if ((s->type == T_OBJECT) && (s->u.ob->flags & O_DESTRUCTED))
              {
                *++sp = const0;
                assign_svalue (s, &const0);
              }
            else
              {
                assign_svalue_no_free (++sp, s);
              }
            break;
          }
        case F_LT:
          f_lt ();
          break;
        case F_ADD:
          {
            switch (sp->type)
              {
              case T_BUFFER:
                {
                  if (!((sp - 1)->type == T_BUFFER))
                    {
                      error ("*Bad type argument to +. Had %s and %s.", type_name ((sp - 1)->type), type_name (sp->type));
                    }
                  else
                    {
                      buffer_t *b;

                      b = allocate_buffer (sp->u.buf->size + (sp - 1)->u.buf->size);
                      memcpy (b->item, (sp - 1)->u.buf->item, (sp - 1)->u.buf->size);
                      memcpy (b->item + (sp - 1)->u.buf->size, sp->u.buf->item, sp->u.buf->size);
                      free_buffer ((sp--)->u.buf);
                      free_buffer (sp->u.buf);
                      sp->u.buf = b;
                    }
                  break;
                }		/* end of x + T_BUFFER */
              case T_NUMBER:
                {
                  switch ((--sp)->type)
                    {
                    case T_NUMBER:
                      sp->u.number += (sp + 1)->u.number;
                      sp->subtype = 0;
                      break;
                    case T_REAL:
                      sp->u.real += (sp + 1)->u.number;
                      break;
                    case T_STRING:
                      {
                        char buff[30];

                        sprintf (buff, "%" PRId64, (sp + 1)->u.number);
                        EXTEND_SVALUE_STRING (sp, buff, "f_add: 2");
                        break;
                      }
                    default:
                      error ("*Bad type argument to +.  Had %s and %s.", type_name (sp->type), type_name ((sp + 1)->type));
                    }
                  break;
                }		/* end of x + NUMBER */
              case T_REAL:
                {
                  switch ((--sp)->type)
                    {
                    case T_NUMBER:
                      sp->type = T_REAL;
                      sp->u.real = sp->u.number + (sp + 1)->u.real;
                      break;
                    case T_REAL:
                      sp->u.real += (sp + 1)->u.real;
                      break;
                    case T_STRING:
                      {
                        char buff[40];

                        sprintf (buff, "%lf", (sp + 1)->u.real);
                        EXTEND_SVALUE_STRING (sp, buff, "f_add: 2");
                        break;
                      }
                    default:
                      error ("*Bad type argument to +. Had %s and %s", type_name (sp->type), type_name ((sp + 1)->type));
                    }
                  break;
                }		/* end of x + T_REAL */
              case T_ARRAY:
                {
                  if (!((sp - 1)->type == T_ARRAY))
                    {
                      error ("*Bad type argument to +. Had %s and %s", type_name ((sp - 1)->type), type_name (sp->type));
                    }
                  else
                    {
                      /* add_array now free's the arrays */
                      (sp - 1)->u.arr = add_array ((sp - 1)->u.arr, sp->u.arr);
                      sp--;
                      break;
                    }
                }		/* end of x + T_ARRAY */
              case T_MAPPING:
                {
                  if ((sp - 1)->type == T_MAPPING)
                    {
                      mapping_t *map;

                      map = add_mapping ((sp - 1)->u.map, sp->u.map);
                      free_mapping ((sp--)->u.map);
                      free_mapping (sp->u.map);
                      sp->u.map = map;
                      break;
                    }
                  else
                    error ("*Bad type argument to +. Had %s and %s", type_name ((sp - 1)->type), type_name (sp->type));
                }		/* end of x + T_MAPPING */
              case T_STRING:
                {
                  switch ((sp - 1)->type)
                    {
                    case T_NUMBER:
                      {
                        char buff[30];

                        sprintf (buff, "%" PRId64, (sp - 1)->u.number);
                        SVALUE_STRING_ADD_LEFT (buff, "f_add: 3");
                        break;
                      }		/* end of T_NUMBER + T_STRING */
                    case T_REAL:
                      {
                        char buff[40];

                        sprintf (buff, "%lf", (sp - 1)->u.real);
                        SVALUE_STRING_ADD_LEFT (buff, "f_add: 3");
                        break;
                      }		/* end of T_REAL + T_STRING */
                    case T_STRING:
                      {
                        SVALUE_STRING_JOIN (sp - 1, sp, "f_add: 1");
                        sp--;
                        break;
                      }		/* end of T_STRING + T_STRING */
                    default:
                      error ("*Bad type argument to +. Had %s and %s.", type_name ((sp - 1)->type), type_name (sp->type));
                    }
                  break;
                }		/* end of x + T_STRING */

              default:
                error ("*Bad type argument to +.  Had %s and %s.", type_name ((sp - 1)->type), type_name (sp->type));
              }
            break;
          }
        case F_VOID_ADD_EQ:
        case F_ADD_EQ:
          DEBUG_CHECK (sp->type != T_LVALUE, "non-lvalue argument to +=\n");
          lval = sp->u.lvalue;
          sp--;			/* points to the RHS */
          switch (lval->type)
            {
            case T_STRING:
              if (sp->type == T_STRING)
                {
                  SVALUE_STRING_JOIN (lval, sp, "f_add_eq: 1");
                  opt_trace (TT_EVAL|3, "f_add_eq: \"%s\"", sp->u.string);
                }
              else if (sp->type == T_NUMBER)
                {
                  char buff[30];

                  sprintf (buff, "%" PRId64, sp->u.number);
                  EXTEND_SVALUE_STRING (lval, buff, "f_add_eq: 2");
                }
              else if (sp->type == T_REAL)
                {
                  char buff[40];

                  sprintf (buff, "%lf", sp->u.real);
                  EXTEND_SVALUE_STRING (lval, buff, "f_add_eq: 2");
                }
              else
                {
                  bad_argument (sp, T_STRING | T_NUMBER | T_REAL, 2, instruction);
                }
              break;
            case T_NUMBER:
              if (sp->type == T_NUMBER)
                {
                  lval->u.number += sp->u.number;
                  /* both sides are numbers, no freeing required */
                }
              else if (sp->type == T_REAL)
                {
                  lval->u.number += (long)sp->u.real;
                  /* both sides are numbers, no freeing required */
                }
              else
                {
                  error ("*Left hand side of += is a number (or zero); right side is not a number.");
                }
              break;
            case T_REAL:
              if (sp->type == T_NUMBER)
                {
                  lval->u.real += sp->u.number;
                  /* both sides are numerics, no freeing required */
                }
              if (sp->type == T_REAL)
                {
                  lval->u.real += sp->u.real;
                  /* both sides are numerics, no freeing required */
                }
              else
                {
                  error ("*Left hand side of += is a number (or zero); right side is not a number.");
                }
              break;
            case T_BUFFER:
              if (sp->type != T_BUFFER)
                {
                  bad_argument (sp, T_BUFFER, 2, instruction);
                }
              else
                {
                  buffer_t *b;

                  b = allocate_buffer (lval->u.buf->size + sp->u.buf->size);
                  memcpy (b->item, lval->u.buf->item, lval->u.buf->size);
                  memcpy (b->item + lval->u.buf->size, sp->u.buf->item, sp->u.buf->size);
                  free_buffer (sp->u.buf);
                  free_buffer (lval->u.buf);
                  lval->u.buf = b;
                }
              break;
            case T_ARRAY:
              if (sp->type != T_ARRAY)
                bad_argument (sp, T_ARRAY, 2, instruction);
              else
                {
                  /* add_array now frees the arrays */
                  lval->u.arr = add_array (lval->u.arr, sp->u.arr);
                }
              break;
            case T_MAPPING:
              if (sp->type != T_MAPPING)
                bad_argument (sp, T_MAPPING, 2, instruction);
              else
                {
                  absorb_mapping (lval->u.map, sp->u.map);
                  free_mapping (sp->u.map);	/* free RHS */
                  /* LHS not freed because its being reused */
                }
              break;
            case T_LVALUE_BYTE:
              {
                char c;

                if (sp->type != T_NUMBER)
                  error ("*Bad right type to += of char lvalue.");

                c = *global_lvalue_byte.u.lvalue_byte + (char)sp->u.number;

                if (c == '\0')
                  error ("*Strings cannot contain 0 bytes.");
                *global_lvalue_byte.u.lvalue_byte = c;
              }
              break;
            default:
              bad_arg (1, instruction);
            }

          if (instruction == F_ADD_EQ)
            {			/* not void add_eq */
              assign_svalue_no_free (sp, lval);
            }
          else
            {
              /*
               * but if (void)add_eq then no need to produce an
               * rvalue
               */
              sp--;
            }
          break;
        case F_AND:
          f_and ();
          break;
        case F_AND_EQ:
          f_and_eq ();
          break;
        case F_FUNCTION_CONSTRUCTOR:
          f_function_constructor ();
          break;

        case F_FOREACH: /* start iteration of string/array/mapping svalue */
          {
            int flags = EXTRACT_UCHAR (pc++);

            if (flags & 4) /* mapping */
              {
                CHECK_TYPES (sp, T_MAPPING, 2, F_FOREACH);

                /* push array of keys */
                push_refed_array (mapping_indices (sp->u.map));

                /* push the hidden iterator */
                (++sp)->type = T_NUMBER;
                sp->u.lvalue = (sp - 1)->u.arr->item;
                sp->subtype = (sp - 1)->u.arr->size;

                /* push lvalue for key */
                (++sp)->type = T_LVALUE;
                if (flags & 2)
                  sp->u.lvalue = find_value ((int)(EXTRACT_UCHAR (pc++) + variable_index_offset));
                else
                  sp->u.lvalue = fp + EXTRACT_UCHAR (pc++);
              }
            else if (sp->type == T_STRING) /* string */
              {
                /* push hidden iterator */
                (++sp)->type = T_NUMBER;
                sp->u.lvalue_byte = (unsigned char *) ((sp - 1)->u.string);
                sp->subtype = (unsigned short)SVALUE_STRLEN (sp - 1);
              }
            else /* array */
              {
                CHECK_TYPES (sp, T_ARRAY, 2, F_FOREACH);

                /* push the hidden iterator */
                (++sp)->type = T_NUMBER;
                sp->u.lvalue = (sp - 1)->u.arr->item;
                sp->subtype = (sp - 1)->u.arr->size;
              }

            /* push lvalue for mapping value, string character, or array element */
            (++sp)->type = T_LVALUE;
            if (flags & 1)
              sp->u.lvalue = find_value ((int)(EXTRACT_UCHAR (pc++) + variable_index_offset));
            else
              sp->u.lvalue = fp + EXTRACT_UCHAR (pc++);
            break;
          }
        case F_NEXT_FOREACH: /* assign next foreach lvalue(s) */
          if ((sp - 1)->type == T_LVALUE)
            {
              /* mapping
               * sp - 4: mapping
               * sp - 3: array of keys
               * sp - 2: hidden iterator (u.lvalue = next key, subtype = number of keys left)
               * sp - 1: lvalue for key
               * sp    : lvalue for value
               */
              if ((sp - 2)->subtype--)
                {
                  svalue_t *key = (sp - 2)->u.lvalue++;
                  svalue_t *value = find_in_mapping ((sp - 4)->u.map, key);

                  assign_svalue ((sp - 1)->u.lvalue, key); /* re-assign key lvalue */
                  assign_svalue (sp->u.lvalue, value);     /* re-assign value lvalue */
                  COPY_SHORT (&offset, pc);
                  pc -= offset; /* repeat loop */
                  break;
                }
            }
          else if ((sp - 2)->type == T_STRING)
            {
              /* string
               * sp - 2: string
               * sp - 1: hidden iterator (u.lvalue_byte = next character, subtype = number of characters left)
               * sp    : lvalue for character
               * 
               * [NEOLITH-EXTENSION] If next multibyte character is an UTF-8 character, return unicode code point as number
               * and advance iterator by the multibyte character length.
               * Otherwise, return next character as number and advance iterator by 1.
               */
              if ((sp - 1)->subtype != 0)
                {
                  wchar_t wc;
                  int char_len = mbtowc (&wc, (char *)(sp - 1)->u.lvalue_byte, (sp - 1)->subtype);
                  if (char_len > 1)
                    {
                      /* Multibyte UTF-8 character - return the Unicode code point */
                      free_svalue (sp->u.lvalue, "foreach-string-mb");
                      sp->u.lvalue->type = T_NUMBER;
                      sp->u.lvalue->subtype = 0;
                      sp->u.lvalue->u.number = (int64_t)wc;
                      (sp - 1)->u.lvalue_byte += char_len;
                    }
                  else /* if (char_len == 1) or any invalid utf-8 sequence */
                    {
                      char c = *((sp - 1)->u.lvalue_byte)++;
                      free_svalue (sp->u.lvalue, "foreach-string");
                      sp->u.lvalue->type = T_NUMBER;
                      sp->u.lvalue->subtype = 0;
                      sp->u.lvalue->u.number = c;
                    }
                  mbtowc (NULL, NULL, 0); /* reset conversion state */
                  /* Decrement bytes remaining and continue loop */
                  if (char_len > 0)
                    (sp - 1)->subtype -= (short)char_len;
                  COPY_SHORT (&offset, pc);
                  pc -= offset; /* repeat loop - will check subtype at next iteration */
                  break;
                }
            }
          else
            {
              /* array
               * sp - 2: string or array
               * sp - 1: hidden iterator (u.lvalue = next element, subtype = number of elements left)
               * sp    : lvalue for element
               */
              if ((sp - 1)->subtype--)
                {
                  if ((sp - 2)->type == T_STRING)
                    {
                      free_svalue (sp->u.lvalue, "foreach-string");
                      sp->u.lvalue->type = T_NUMBER;
                      sp->u.lvalue->subtype = 0;
                      sp->u.lvalue->u.number = *((sp - 1)->u.lvalue_byte)++;
                    }
                  else
                    {
                      assign_svalue (sp->u.lvalue, (sp - 1)->u.lvalue++); /* re-assign element lvalue */
                    }
                  COPY_SHORT (&offset, pc);
                  pc -= offset; /* repeat loop */
                  break;
                }
            }
          pc += 2;
          /* fallthrough */
        case F_EXIT_FOREACH:
          if ((sp - 1)->type == T_LVALUE)
            {
              /* mapping */
              sp -= 3;
              free_array ((sp--)->u.arr);
              free_mapping ((sp--)->u.map);
            }
          else
            {
              /* array or string */
              sp -= 2;
              if (sp->type == T_STRING)
                free_string_svalue (sp--);
              else
                free_array ((sp--)->u.arr);
            }
          break;

        case F_EXPAND_VARARGS:
          {
            svalue_t *s, *t;
            array_t *arr;

            i = EXTRACT_UCHAR (pc++);
            s = sp - i;

            if (s->type != T_ARRAY)
              error ("*Item being expanded with ... is not an array.");

            arr = s->u.arr;
            n = arr->size;
            num_varargs += n - 1;
            if (!n)
              {
                t = s;
                while (t < sp)
                  {
                    *t = *(t + 1);
                    t++;
                  }
                sp--;
              }
            else if (n == 1)
              {
                assign_svalue_no_free (s, &arr->item[0]);
              }
            else
              {
                t = sp;
                sp += n - 1;
                while (t > s)
                  {
                    *(t + n - 1) = *t;
                    t--;
                  }
                t = s + n - 1;
                if (arr->ref == 1)
                  {
                    memcpy (s, arr->item, n * sizeof (svalue_t));
                    free_empty_array (arr);
                    break;
                  }
                else
                  {
                    while (n--)
                      assign_svalue_no_free (t--, &arr->item[n]);
                  }
              }
            free_array (arr);
            break;
          }

        case F_NEW_CLASS:
          {
            array_t *cl;

            cl = allocate_class (&current_prog->classes[EXTRACT_UCHAR (pc++)], 1);
            push_refed_class (cl);
          }
          break;
        case F_NEW_EMPTY_CLASS:
          {
            array_t *cl;

            cl = allocate_class (&current_prog->classes[EXTRACT_UCHAR (pc++)], 0);
            push_refed_class (cl);
          }
          break;
        case F_AGGREGATE:
          {
            array_t *v;

            LOAD_SHORT (offset, pc);
            offset += (unsigned short)num_varargs;
            num_varargs = 0;
            v = allocate_empty_array ((int) offset);
            /*
             * transfer svalues in reverse...popping stack as we go
             */
            while (offset--)
              v->item[offset] = *sp--;
            (++sp)->type = T_ARRAY;
            sp->u.arr = v;
          }
          break;
        case F_AGGREGATE_ASSOC:
          {
            mapping_t *m;

            LOAD_SHORT (offset, pc);

            offset += (unsigned short)num_varargs;
            num_varargs = 0;
            m = load_mapping_from_aggregate (sp -= offset, offset);
            (++sp)->type = T_MAPPING;
            sp->u.map = m;
            break;
          }
        case F_ASSIGN:
          switch (sp->u.lvalue->type)
            {
            case T_LVALUE_BYTE:
              {
                char c;

                if ((sp - 1)->type != T_NUMBER)
                  {
                    error ("*Illegal to assign a non-numeric value to char lvalue.");
                  }
                else
                  {
                    c = ((sp - 1)->u.number & 0xff);
                    if (c == '\0')
                      error ("*Strings cannot contain NUL character.");
                    *global_lvalue_byte.u.lvalue_byte = c;
                  }
                break;
              }
            default:
              assign_svalue (sp->u.lvalue, sp - 1);
              break;
            case T_LVALUE_RANGE:
              assign_lvalue_range (sp - 1);
              break;
            }
          sp--;			/* ignore lvalue */
          /* rvalue is already in the correct place */
          break;
        case F_VOID_ASSIGN_LOCAL:
          if (sp->type != T_INVALID)
            {
              lval = fp + EXTRACT_UCHAR (pc++);
              free_svalue (lval, "F_VOID_ASSIGN_LOCAL");
              *lval = *sp--;
            }
          else
            {
              sp--;
              pc++;
            }
          break;
        case F_VOID_ASSIGN:
          lval = (sp--)->u.lvalue;
          if (sp->type != T_INVALID)
            {
              switch (lval->type)
                {
                case T_LVALUE_BYTE:
                  {
                    if (sp->type != T_NUMBER)
                      {
                        error ("*Illegal rhs to char lvalue");
                      }
                    else
                      {
                        char c = (sp--)->u.number & 0xff;
                        if (c == '\0')
                          error ("*Strings cannot contain 0 bytes.");
                        *global_lvalue_byte.u.lvalue_byte = c;
                      }
                    break;
                  }

                case T_LVALUE_RANGE:
                  {
                    copy_lvalue_range (sp--);
                    break;
                  }

                default:
                  {
                    free_svalue (lval, "F_VOID_ASSIGN : 3");
                    *lval = *sp--;
                  }
                }
            }
          else
            sp--;
          break;
        case F_CALL_FUNCTION_BY_ADDRESS:
          {
            compiler_function_t *funp;
            const char* name;

            LOAD_SHORT (offset, pc);
            offset += (unsigned short)function_index_offset;
            /*
             * Find the function in the function table. As the
             * function may have been redefined by inheritance, we
             * must look in the last table, which is pointed to by
             * current_object.
             */
            if (offset >= current_object->prog->num_functions_total)
                error ("illegal function index");

            name = function_name (current_object->prog, offset);
            if (current_object->prog->function_flags[offset] & NAME_UNDEFINED)
              error ("undefined function: %s", name);


            /* Save all important global stack machine registers */
            push_control_stack (FRAME_FUNCTION);
            current_prog = current_object->prog;

            caller_type = ORIGIN_LOCAL;
            /*
             * If it is an inherited function, search for the real
             * definition.
             */
            csp->num_local_variables = EXTRACT_UCHAR (pc++) + num_varargs;
            num_varargs = 0;
            funp = setup_new_frame (offset);
            csp->pc = pc;	/* The corrected return address of local function call */
            pc = current_prog->program + funp->address; // F_CALL_FUNCTION_BY_ADDRESS
            opt_trace (TT_EVAL, "call_function_by_address \"%s\": offset %+d", name, funp->address);
          }
          break;
        case F_CALL_INHERITED:
          {
            inherit_t *ip = current_prog->inherit + EXTRACT_UCHAR (pc++);
            program_t *temp_prog = ip->prog;
            compiler_function_t *funp;

            LOAD_SHORT (offset, pc);

            push_control_stack (FRAME_FUNCTION);
            current_prog = temp_prog;

            caller_type = ORIGIN_LOCAL;

            csp->num_local_variables = EXTRACT_UCHAR (pc++) + num_varargs;
            num_varargs = 0;

            function_index_offset += ip->function_index_offset;
            variable_index_offset += ip->variable_index_offset;

            funp = setup_inherited_frame (offset);
            csp->pc = pc;
            pc = current_prog->program + funp->address; // F_CALL_INHERITED
            opt_trace (TT_EVAL, "call_inherited \"%s\": offset %+d", funp->name, funp->address);
          }
          break;
        case F_COMPL:
          if (sp->type != T_NUMBER)
            error ("*Bad argument to ~");
          sp->u.number = ~sp->u.number;
          sp->subtype = 0;
          break;
        case F_CONST0:
          push_number (0);
          break;
        case F_CONST1:
          push_number (1);
          break;
        case F_PRE_DEC:
          DEBUG_CHECK (sp->type != T_LVALUE, "non-lvalue argument to --\n");
          lval = sp->u.lvalue;
          switch (lval->type)
            {
            case T_NUMBER:
              sp->type = T_NUMBER;
              sp->subtype = 0;
              sp->u.number = --(lval->u.number);
              break;
            case T_REAL:
              sp->type = T_REAL;
              sp->u.real = --(lval->u.real);
              break;
            case T_LVALUE_BYTE:
              if (*global_lvalue_byte.u.lvalue_byte == '\x1')
                error ("*Strings cannot contain 0 bytes.");
              sp->type = T_NUMBER;
              sp->subtype = 0;
              sp->u.number = --(*global_lvalue_byte.u.lvalue_byte);
              break;
            default:
              error ("Decrement (--) on non-numeric argument");
            }
          break;
        case F_DEC:
          DEBUG_CHECK (sp->type != T_LVALUE, "non-lvalue argument to --\n");
          lval = (sp--)->u.lvalue;
          switch (lval->type)
            {
            case T_NUMBER:
              lval->u.number--;
              break;
            case T_REAL:
              lval->u.real--;
              break;
            case T_LVALUE_BYTE:
              if (*global_lvalue_byte.u.lvalue_byte == '\x1')
                error ("*Strings cannot contain NUL char.");
              --(*global_lvalue_byte.u.lvalue_byte);
              break;
            default:
              error ("Decrement (--) on non-numeric argument");
            }
          break;
        case F_DIVIDE:
          {
            switch ((sp - 1)->type | sp->type)
              {

              case T_NUMBER:
                {
                  if (!(sp--)->u.number)
                    error ("*Division by zero.");
                  sp->u.number /= (sp + 1)->u.number;
                  break;
                }

              case T_REAL:
                {
                  if ((sp--)->u.real == 0.0)
                    error ("*Division by zero.");
                  sp->u.real /= (sp + 1)->u.real;
                  break;
                }

              case T_NUMBER | T_REAL:
                {
                  if ((sp--)->type == T_NUMBER)
                    {
                      if (!((sp + 1)->u.number))
                        error ("*Division by zero.");
                      sp->u.real /= (sp + 1)->u.number;
                    }
                  else
                    {
                      if ((sp + 1)->u.real == 0.0)
                        error ("*Division by 0.0");
                      sp->type = T_REAL;
                      sp->u.real = sp->u.number / (sp + 1)->u.real;
                    }
                  break;
                }

              default:
                {
                  if (!((sp - 1)->type & (T_NUMBER | T_REAL)))
                    bad_argument (sp - 1, T_NUMBER | T_REAL, 1, instruction);
                  if (!(sp->type & (T_NUMBER | T_REAL)))
                    bad_argument (sp, T_NUMBER | T_REAL, 2, instruction);
                }
              }
          }
          break;
        case F_DIV_EQ:
          f_div_eq ();
          break;
        case F_EQ:
          f_eq ();
          break;
        case F_GE:
          f_ge ();
          break;
        case F_GT:
          f_gt ();
          break;
        case F_GLOBAL:
          {
            svalue_t *s;

            s = find_value ((int)(EXTRACT_UCHAR (pc++) + variable_index_offset));

            /*
             * If variable points to a destructed object, replace it
             * with 0, otherwise, fetch value of variable.
             */
            if ((s->type == T_OBJECT) && (s->u.ob->flags & O_DESTRUCTED))
              {
                *++sp = const0;
                assign_svalue (s, &const0);
              }
            else
              {
                assign_svalue_no_free (++sp, s);
              }
            break;
          }
        case F_PRE_INC:
          DEBUG_CHECK (sp->type != T_LVALUE, "non-lvalue argument to ++\n");
          lval = sp->u.lvalue;
          switch (lval->type)
            {
            case T_NUMBER:
              sp->type = T_NUMBER;
              sp->subtype = 0;
              sp->u.number = ++lval->u.number;
              break;
            case T_REAL:
              sp->type = T_REAL;
              sp->u.real = (double)++lval->u.number;
              break;
            case T_LVALUE_BYTE:
              if (*global_lvalue_byte.u.lvalue_byte == (unsigned char) 255)
                error ("*Strings cannot contain NUL char.");
              sp->type = T_NUMBER;
              sp->subtype = 0;
              sp->u.number = ++*global_lvalue_byte.u.lvalue_byte;
              break;
            default:
              error ("Increment (++) on non-numeric argument.");
            }
          break;
        case F_MEMBER:
          {
            array_t *arr;

            if (sp->type != T_CLASS)
              error("*Tried to take a member of something that isn't a class.");
            i = EXTRACT_UCHAR (pc++);
            arr = sp->u.arr;
            if (i >= arr->size)
              error ("*Class has no corresponding member.");
            assign_svalue_no_free (sp, &arr->item[i]);
            free_class (arr);

            /*
             * Fetch value of a variable. It is possible that it is a
             * variable that points to a destructed object. In that case,
             * it has to be replaced by 0.
             */
            if (sp->type == T_OBJECT && (sp->u.ob->flags & O_DESTRUCTED))
              {
                free_object (sp->u.ob, "F_MEMBER");
                sp->type = T_NUMBER;
                sp->u.number = 0;
              }
            break;
          }
        case F_MEMBER_LVALUE:
          {
            array_t *arr;

            if (sp->type != T_CLASS)
              error ("*Tried to take a member of something that isn't a class.");
            i = EXTRACT_UCHAR (pc++);
            arr = sp->u.arr;
            if (i >= arr->size)
              error ("*Class has no corresponding member.");
            sp->type = T_LVALUE;
            sp->u.lvalue = arr->item + i;
            free_class (arr);
            break;
          }
        case F_INDEX:
          switch (sp->type)
            {
            case T_MAPPING:
              {
                svalue_t *v;
                mapping_t *m;

                v = find_in_mapping (m = sp->u.map, sp - 1);
                assign_svalue (--sp, v);	/* v will always have a * value */
                free_mapping (m);
                break;
              }
            case T_BUFFER:
              {
                if ((sp - 1)->type != T_NUMBER)
                  error ("*Buffer indexes must be integers.");

                i = (int)(sp - 1)->u.number;
                if ((i > (int)sp->u.buf->size) || (i < 0))
                  error ("*Buffer index out of bounds.");
                i = sp->u.buf->item[i];
                free_buffer (sp->u.buf);
                (--sp)->u.number = i;
                sp->subtype = 0;
                break;
              }
            case T_STRING:
              {
                if ((sp - 1)->type != T_NUMBER)
                  {
                    error ("*String indexes must be integers.");
                  }
                i = (int)(sp - 1)->u.number;
                if ((i > (int)SVALUE_STRLEN (sp)) || (i < 0))
                  error ("*String index out of bounds.");
                i = (unsigned char) sp->u.string[i];
                free_string_svalue (sp);
                (--sp)->u.number = i;
                break;
              }
            case T_ARRAY:
              {
                array_t *arr;

                if ((sp - 1)->type != T_NUMBER)
                  error ("*Array indexes must be integers.");
                i = (int)(sp - 1)->u.number;
                if (i < 0)
                  error ("*Array index must be positive or zero.");
                arr = sp->u.arr;
                if (i >= arr->size)
                  error ("*Array index out of bounds.");
                assign_svalue_no_free (--sp, &arr->item[i]);
                free_array (arr);
                break;
              }
            default:
              if (sp->type == T_NUMBER && !sp->u.number)
                error ("*Value being indexed is zero.");
              error ("*Cannot index value of type '%s'.", type_name (sp->type));
            }

          /*
           * Fetch value of a variable. It is possible that it is a
           * variable that points to a destructed object. In that case,
           * it has to be replaced by 0.
           */
          if (sp->type == T_OBJECT && (sp->u.ob->flags & O_DESTRUCTED))
            {
              free_object (sp->u.ob, "F_INDEX");
              sp->type = T_NUMBER;
              sp->u.number = 0;
            }
          break;
        case F_RINDEX:
          switch (sp->type)
            {
            case T_BUFFER:
              {
                if ((sp - 1)->type != T_NUMBER)
                  error ("*Indexing a buffer with an illegal type.");

                i = sp->u.buf->size - (int)(sp - 1)->u.number;
                if ((i > (int)sp->u.buf->size) || (i < 0))
                  error ("*Buffer index out of bounds.");

                i = sp->u.buf->item[i];
                free_buffer (sp->u.buf);
                (--sp)->u.number = i;
                sp->subtype = 0;
                break;
              }
            case T_STRING:
              {
                size_t len = SVALUE_STRLEN (sp);
                if ((sp - 1)->type != T_NUMBER)
                  {
                    error ("*Indexing a string with an illegal type.");
                  }
                i = (int)(len - (sp - 1)->u.number);
                if ((i > (int)len) || (i < 0))
                  error ("*String index out of bounds.");
                i = (unsigned char) sp->u.string[i];
                free_string_svalue (sp);
                (--sp)->u.number = i;
                break;
              }
            case T_ARRAY:
              {
                array_t *arr = sp->u.arr;

                if ((sp - 1)->type != T_NUMBER)
                  error ("*Indexing an array with an illegal type.");
                i = arr->size - (int)(sp - 1)->u.number;
                if (i < 0 || i >= (int)(arr->size))
                  error ("*Array index out of bounds.");
                assign_svalue_no_free (--sp, &arr->item[i]);
                free_array (arr);
                break;
              }
            default:
              if (sp->type == T_NUMBER && !sp->u.number)
                error ("*Value being indexed is zero.");
              error ("*Cannot index value of type '%s'.", type_name (sp->type));
            }

          /*
           * Fetch value of a variable. It is possible that it is a
           * variable that points to a destructed object. In that case,
           * it has to be replaced by 0.
           */
          if (sp->type == T_OBJECT && (sp->u.ob->flags & O_DESTRUCTED))
            {
              free_object (sp->u.ob, "F_RINDEX");
              sp->type = T_NUMBER;
              sp->u.number = 0;
            }
          break;
#ifdef F_JUMP_WHEN_ZERO
        case F_JUMP_WHEN_ZERO:
          if ((i = (sp->type == T_NUMBER)) && sp->u.number == 0)
            {
              COPY_SHORT (&offset, pc);
              pc = current_prog->program + offset; // F_JUMP_WHEN_ZERO
            }
          else
            {
              pc += 2;
            }
          if (i)
            {
              sp--;		/* cheaper to do this when sp is an integer
                                 * svalue */
            }
          else
            {
              pop_stack ();
            }
          break;
#endif
#ifdef F_JUMP
        case F_JUMP:
          COPY_SHORT (&offset, pc);
          pc = current_prog->program + offset; // F_JUMP
          break;
#endif
        case F_LE:
          f_le ();
          break;
        case F_LSH:
          f_lsh ();
          break;
        case F_LSH_EQ:
          f_lsh_eq ();
          break;
        case F_MOD:
          {
            CHECK_TYPES (sp - 1, T_NUMBER, 1, instruction);
            CHECK_TYPES (sp, T_NUMBER, 2, instruction);
            if ((sp--)->u.number == 0)
              error ("*Modulus by zero.");
            sp->u.number %= (sp + 1)->u.number;
          }
          break;
        case F_MOD_EQ:
          f_mod_eq ();
          break;
        case F_MULTIPLY:
          {
            switch ((sp - 1)->type | sp->type)
              {
              case T_NUMBER:
                {
                  sp--;
                  sp->u.number *= (sp + 1)->u.number;
                  break;
                }

              case T_REAL:
                {
                  sp--;
                  sp->u.real *= (sp + 1)->u.real;
                  break;
                }

              case T_NUMBER | T_REAL:
                {
                  if ((--sp)->type == T_NUMBER)
                    {
                      sp->type = T_REAL;
                      sp->u.real = sp->u.number * (sp + 1)->u.real;
                    }
                  else
                    sp->u.real *= (sp + 1)->u.number;
                  break;
                }

              case T_MAPPING:
                {
                  mapping_t *m;
                  m = compose_mapping ((sp - 1)->u.map, sp->u.map, 1);
                  pop_2_elems ();
                  (++sp)->type = T_MAPPING;
                  sp->u.map = m;
                  break;
                }

              default:
                {
                  if (!((sp - 1)->type & (T_NUMBER | T_REAL | T_MAPPING)))
                    bad_argument (sp - 1, T_NUMBER | T_REAL | T_MAPPING, 1, instruction);
                  if (!(sp->type & (T_NUMBER | T_REAL | T_MAPPING)))
                    bad_argument (sp, T_NUMBER | T_REAL | T_MAPPING, 2, instruction);
                  error ("*Args to * are not compatible.");
                }
              }
          }
          break;
        case F_MULT_EQ:
          f_mult_eq ();
          break;
        case F_NE:
          f_ne ();
          break;
        case F_NEGATE:
          if (sp->type == T_NUMBER)
            {
              sp->subtype = 0;
              sp->u.number = -sp->u.number;
            }
          else if (sp->type == T_REAL)
            sp->u.real = -sp->u.real;
          else
            error ("*Bad argument to unary minus");
          break;
        case F_NOT:
          if (sp->type == T_NUMBER)
            {
              sp->subtype = 0;
              sp->u.number = !sp->u.number;
            }
          else
            assign_svalue (sp, &const0);
          break;
        case F_OR:
          f_or ();
          break;
        case F_OR_EQ:
          f_or_eq ();
          break;
        case F_PARSE_COMMAND:
          f_parse_command ();
          break;
        case F_POP_VALUE:
          pop_stack ();
          break;
        case F_POST_DEC:
          DEBUG_CHECK (sp->type != T_LVALUE, "non-lvalue argument to --\n");
          lval = sp->u.lvalue;
          switch (lval->type)
            {
            case T_NUMBER:
              sp->type = T_NUMBER;
              sp->subtype = 0;
              sp->u.number = lval->u.number--;
              break;
            case T_REAL:
              sp->type = T_REAL;
              sp->u.real = lval->u.real--;
              break;
            case T_LVALUE_BYTE:
              sp->type = T_NUMBER;
              if (*global_lvalue_byte.u.lvalue_byte == '\x1')
                error ("*Strings cannot contain NUL char.");
              sp->u.number = (*global_lvalue_byte.u.lvalue_byte)--;
              break;
            default:
              error ("DEcrement (--) on non-numeric argument.");
            }
          break;
        case F_POST_INC:
          DEBUG_CHECK (sp->type != T_LVALUE, "non-lvalue argument to ++\n");
          lval = sp->u.lvalue;
          switch (lval->type)
            {
            case T_NUMBER:
              sp->type = T_NUMBER;
              sp->subtype = 0;
              sp->u.number = lval->u.number++;
              break;
            case T_REAL:
              sp->type = T_REAL;
              sp->u.real = lval->u.real++;
              break;
            case T_LVALUE_BYTE:
              if (*global_lvalue_byte.u.lvalue_byte == (unsigned char) 255)
                error ("*Strings cannot contain NUL char.");
              sp->type = T_NUMBER;
              sp->u.number = (*global_lvalue_byte.u.lvalue_byte)++;
              break;
            default:
              error ("Increment (++) on non-numeric argument.");
            }
          break;
        case F_GLOBAL_LVALUE:
          (++sp)->type = T_LVALUE;
          sp->u.lvalue = find_value ((int) (EXTRACT_UCHAR (pc++) + variable_index_offset));
          break;
        case F_INDEX_LVALUE:
          push_indexed_lvalue (0);
          break;
        case F_RINDEX_LVALUE:
          push_indexed_lvalue (1);
          break;
        case F_NN_RANGE_LVALUE:
          push_lvalue_range (0x00);
          break;
        case F_RN_RANGE_LVALUE:
          push_lvalue_range (0x10);
          break;
        case F_RR_RANGE_LVALUE:
          push_lvalue_range (0x11);
          break;
        case F_NR_RANGE_LVALUE:
          push_lvalue_range (0x01);
          break;
        case F_NN_RANGE:
          f_range (0x00);
          break;
        case F_RN_RANGE:
          f_range (0x10);
          break;
        case F_NR_RANGE:
          f_range (0x01);
          break;
        case F_RR_RANGE:
          f_range (0x11);
          break;
        case F_NE_RANGE:
          f_extract_range (0);
          break;
        case F_RE_RANGE:
          f_extract_range (1);
          break;
        case F_RETURN_ZERO:
          {
            /*
             * Deallocate frame and return.
             */
            pop_n_elems (csp->num_local_variables);
            sp++;
            DEBUG_CHECK (sp != fp, "Bad stack at F_RETURN\n");
            *sp = const0;
            pop_control_stack ();
            /* The control stack was popped just before */
            if (csp[1].framekind & FRAME_EXTERNAL)
              return;
            break;
          }
          break;
        case F_RETURN:
          {
            svalue_t sv;

            if (csp->num_local_variables)
              {
                sv = *sp--;
                /*
                 * Deallocate frame and return.
                 */
                pop_n_elems (csp->num_local_variables);
                sp++;
                DEBUG_CHECK (sp != fp, "Bad stack at F_RETURN\n");
                *sp = sv;	/* This way, the same ref counts are maintained */
              }
            opt_trace (TT_EVAL|2, "returning \"%s\"", sp->type == T_STRING ? sp->u.string : "(non-string)");
            pop_control_stack ();
            /* The control stack was popped just before */
            if (csp[1].framekind & FRAME_EXTERNAL)
              return;
            break;
          }
        case F_RSH:
          f_rsh ();
          break;
        case F_RSH_EQ:
          f_rsh_eq ();
          break;
        case F_SSCANF:
          f_sscanf ();
          break;
        case F_STRING:
          LOAD_SHORT (offset, pc);
          DEBUG_CHECK1 (offset >= current_prog->num_strings, "string %d out of range in F_STRING!\n", offset);
          push_shared_string (current_prog->strings[offset]);
          break;
        case F_SHORT_STRING:
          DEBUG_CHECK1 (EXTRACT_UCHAR (pc) >= current_prog->num_strings, "string %d out of range in F_STRING!\n", EXTRACT_UCHAR (pc));
          push_shared_string (current_prog->strings[EXTRACT_UCHAR (pc++)]);
          break;
        case F_SUBTRACT:
          {
            i = (sp--)->type;
            switch (i | sp->type)
              {
              case T_NUMBER:
                sp->u.number -= (sp + 1)->u.number;
                break;

              case T_REAL:
                sp->u.real -= (sp + 1)->u.real;
                break;

              case T_NUMBER | T_REAL:
                if (sp->type == T_REAL)
                  sp->u.real -= (sp + 1)->u.number;
                else
                  {
                    sp->type = T_REAL;
                    sp->u.real = sp->u.number - (sp + 1)->u.real;
                  }
                break;

              case T_ARRAY:
                {
                  /*
                   * subtract_array already takes care of
                   * destructed objects
                   */
                  sp->u.arr = subtract_array (sp->u.arr, (sp + 1)->u.arr);
                  break;
                }

              default:
                if (!((sp++)->type & (T_NUMBER | T_REAL | T_ARRAY)))
                  error ("*Bad left type to -.");
                else if (!(sp->type & (T_NUMBER | T_REAL | T_ARRAY)))
                  error ("*Bad right type to -.");
                else
                  error ("*Arguments to - do not have compatible types.");
              }
            break;
          }
        case F_SUB_EQ:
          f_sub_eq ();
          break;
        case F_SIMUL_EFUN:
          {
            unsigned short index;
            int num_args;

            LOAD_SHORT (index, pc);
            num_args = EXTRACT_UCHAR (pc++) + num_varargs;
            num_varargs = 0;
            call_simul_efun (index, num_args);
          }
          break;
        case F_SWITCH:
          f_switch ();
          break;
        case F_XOR:
          f_xor ();
          break;
        case F_XOR_EQ:
          f_xor_eq ();
          break;
        case F_CATCH:
          {
            /*
             * Compute address of next instruction after the CATCH
             * statement.
             */
            ((char *) &offset)[0] = pc[0];
            ((char *) &offset)[1] = pc[1];
            offset = (unsigned short)(pc + offset - current_prog->program);
            pc += 2;

            do_catch (pc, offset);
            pc = current_prog->program + offset; // F_CATCH
            break;
          }
        case F_END_CATCH:
          {
            free_svalue (&catch_value, "F_END_CATCH");
            catch_value = const0;
            /* We come here when no longjmp() was executed */
            pop_control_stack ();
            push_number (0);
            return;		/* return to do_catch */
          }
        case F_TIME_EXPRESSION:
          {
            struct timeval tv;

            gettimeofday (&tv, NULL);
            push_number (tv.tv_sec);
            push_number (tv.tv_usec);
            break;
          }
        case F_END_TIME_EXPRESSION:
          {
            struct timeval tv;
            long usec;

            gettimeofday (&tv, NULL);
            usec = (tv.tv_sec - (long)(sp - 1)->u.number) * 1000000 + (tv.tv_usec - (long)sp->u.number);
            sp -= 2;
            push_number (usec);
            break;
          }
#define Instruction (instruction + ONEARG_MAX)
#define CALL_THE_EFUN(i) (*efun_table[i - BASE + ONEARG_MAX])() 
        case F_EFUN0:
          st_num_arg = 0;
          instruction = EXTRACT_UCHAR (pc++);
          CALL_THE_EFUN(instruction);
          continue;
        case F_EFUN1:
          st_num_arg = 1;
          instruction = EXTRACT_UCHAR (pc++);
          CHECK_TYPES (sp, instrs2[instruction].type[0], 1, Instruction);
          CALL_THE_EFUN(instruction);
          continue;
        case F_EFUN2:
          st_num_arg = 2;
          instruction = EXTRACT_UCHAR (pc++);
          CHECK_TYPES (sp - 1, instrs2[instruction].type[0], 1, Instruction);
          CHECK_TYPES (sp, instrs2[instruction].type[1], 2, Instruction);
          CALL_THE_EFUN(instruction);
          continue;
        case F_EFUN3:
          st_num_arg = 3;
          instruction = EXTRACT_UCHAR (pc++);
          CHECK_TYPES (sp - 2, instrs2[instruction].type[0], 1, Instruction);
          CHECK_TYPES (sp - 1, instrs2[instruction].type[1], 2, Instruction);
          CHECK_TYPES (sp, instrs2[instruction].type[2], 3, Instruction);
          CALL_THE_EFUN(instruction);
          continue;
        case F_EFUNV:
          {
            int num;
            st_num_arg = EXTRACT_UCHAR (pc++) + num_varargs;
            num_varargs = 0;
            instruction = EXTRACT_UCHAR (pc++);
            num = instrs2[instruction].min_arg;
            for (i = 1; i <= num; i++)
              {
                CHECK_TYPES (sp - st_num_arg + i, instrs2[instruction].type[i - 1], i, Instruction);
              }
            CALL_THE_EFUN(instruction);
            continue;
          }
        default:
          /* optimized 1 arg efun */
          st_num_arg = 1;
          CHECK_TYPES (sp, instrs[instruction].type[0], 1, instruction);
          (*efun_table[instruction - BASE]) ();
          continue;
        }			/* switch (instruction) */
      DEBUG_CHECK1 (sp < fp + csp->num_local_variables - 1, "Bad stack after evaluation. Instruction %d\n", instruction);
    }				/* while (1) */
}

void call_efun(int instruction) {
  (*efun_table[instruction - BASE]) ();
}

#ifndef NO_SHADOWS
/*
   is_static: returns 1 if a function named 'fun' is declared 'static' in 'ob';
   0 otherwise.
   */
int
is_static (const char *fun, object_t * ob)
{
  int index;
  int runtime_index;
  program_t *prog;
  compiler_function_t *cfp;

  DEBUG_CHECK (ob->flags & O_DESTRUCTED,
               "is_static() on destructed object\n");

  prog = find_function_by_name (ob, fun, &index, &runtime_index);
  if (!prog)
    return 0;

  cfp = prog->function_table + index;

  if (ob->prog->function_flags[runtime_index] & (NAME_UNDEFINED))
    return 0;
  if (ob->prog->function_flags[runtime_index] & (NAME_STATIC))
    return 1;

  return 0;
}
#endif

/**
 *  @brief Call a specific function address in an object.
 *  This is done with no frame set up.
 *  Arguments must already be pushed on the stack.
 *  Returned values are removed automatically unless ret_value is non-NULL.
 *  This was used by heart_beat() only in LPMud and MudOS.
 * 
 *  [NEOLITH-EXTENSION] If current_heart_beat is set, it will be used as the current object.
 *  Otherwise, a dummy object is created for the call.
 *  @param progp The program containing the function.
 *  @param runtime_index The runtime function index to call.
 *  @param num_args Number of arguments already pushed on the stack.
 *  @param ret_value Where to store the return value, or NULL if none. The caller is responsible for
 *  freeing it if needed.
 */
void call_function (program_t *progp, int runtime_index, int num_args, svalue_t *ret_value) {
  object_t dummy_ob;
  compiler_function_t *funp;

  if ((runtime_index < 0) || (runtime_index > progp->num_functions_total) || (progp->function_flags[runtime_index] & NAME_UNDEFINED))
    {
      if (ret_value)
        *ret_value = const0u;
      return;
    }
  push_control_stack (FRAME_FUNCTION | FRAME_OB_CHANGE);
  caller_type = ORIGIN_DRIVER;
  DEBUG_CHECK (csp != control_stack, "call_function with bad csp\n");
  csp->num_local_variables = num_args;
  current_prog = progp;
  funp = setup_new_frame (runtime_index);
  previous_ob = current_object;
  if (current_heart_beat)
    current_object = current_heart_beat;
  else if (!current_object)
    {
      /* Create a dummy object for the call (no global variables) */
      memset (&dummy_ob, 0, sizeof (dummy_ob));
      dummy_ob.ref = 1;
      dummy_ob.prog = progp;
      current_object = &dummy_ob;
    }
  call_program (current_prog, funp->address);
  if (ret_value)
    *ret_value = *sp--;
  else
    pop_stack ();
}
