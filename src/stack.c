#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "apply.h"
#include "interpret.h"
#include "lpc/array.h"
#include "lpc/buffer.h"
#include "lpc/class.h"
#include "lpc/mapping.h"
#include "lpc/object.h"
#include "rc/rc.h"

svalue_t *fp;           /* Pointer to first argument. */
svalue_t *sp;           /* Pointer to stack top. */
int st_num_arg;         /* Number of arguments to current function. */

svalue_t *start_of_stack = 0;
svalue_t *end_of_stack;

svalue_t const0, const1, const0u;

#define CHECK_AND_PUSH(n)	do {\
        if ((sp += n) >= end_of_stack) \
          { sp -= n; set_error_state(ES_STACK_FULL); error("***Stack overflow!"); } \
        } while (0)

/**
 * Reset the virtual stack machine.
 */
void reset_interpreter (void) {
  static int _init = 0;

  const0.type = T_NUMBER;
  const0.u.number = 0;
  const1.type = T_NUMBER;
  const1.u.number = 1;
  const0u.type = T_NUMBER;
  const0u.subtype = T_UNDEFINED;
  const0u.u.number = 0;

  free_svalue (&apply_ret_value, "reset_interpreter");
  apply_ret_value = const0u;

  csp = control_stack - 1;

  if (!_init)
    {
      int size = CONFIG_INT (__EVALUATOR_STACK_SIZE__);

      start_of_stack = (svalue_t *) calloc (size, sizeof (svalue_t));
      end_of_stack = start_of_stack + size - 5;
      sp = start_of_stack - 1;

      control_stack = (control_stack_t *) calloc (CONFIG_INT (__MAX_CALL_DEPTH__), sizeof (control_stack_t));
      csp = control_stack - 1;

      if (!start_of_stack || !control_stack)
        {
          debug_message ("***** Failed allocating LPC stacks, server will shutdown.\n");
          exit (EXIT_FAILURE);
        }
      _init = 1;
    }
  else
    {
      pop_n_elems (sp - start_of_stack + 1);
    }
}

/*
 * Push an object pointer on the stack. Note that the reference count is * incremented.
 * A destructed object must never be pushed onto the stack.
 */
void push_object (object_t * ob) {
  CHECK_AND_PUSH(1);
  sp->type = T_OBJECT;
  sp->u.ob = ob;
  add_ref (ob, "push_object");
}

/** Push a number on the value stack. */
void push_number (int64_t n) {
  CHECK_AND_PUSH(1);
  sp->type = T_NUMBER;
  sp->subtype = 0;
  sp->u.number = n;
}

void push_real (double n) {
  CHECK_AND_PUSH(1);
  sp->type = T_REAL;
  sp->u.real = n;
}

void push_undefined () {
  CHECK_AND_PUSH(1);
  *sp = const0u;
}

void push_undefineds (int num) {
  STACK_CHECK(num);
  while (num--)
    *++sp = const0u;
}

void copy_and_push_string (const char *p) {
  CHECK_AND_PUSH(1);
  sp->type = T_STRING;
  sp->subtype = STRING_MALLOC;
  sp->u.string = string_copy (p, "copy_and_push_string");
}

void share_and_push_string (const char *p) {
  CHECK_AND_PUSH(1);
  sp->type = T_STRING;
  sp->subtype = STRING_SHARED;
  sp->u.string = make_shared_string (p);
}

void push_some_svalues (svalue_t * v, int num) {
  STACK_CHECK (num);
  while (num--)
    push_svalue (v++);
}

void transfer_push_some_svalues (svalue_t * v, int num) {
  STACK_CHECK (num);
  memcpy (sp + 1, v, num * sizeof (svalue_t));
  sp += num;
}


/*
 * Push a pointer to a array on the stack. Note that the reference count
 * is incremented. Newly created arrays normally have a reference count
 * initialized to 1.
 */
void
push_array (array_t * v)
{
  v->ref++;
  sp++;
  sp->type = T_ARRAY;
  sp->u.arr = v;
}

void
push_refed_array (array_t * v)
{
  sp++;
  sp->type = T_ARRAY;
  sp->u.arr = v;
}

void
push_buffer (buffer_t * b)
{
  b->ref++;
  sp++;
  sp->type = T_BUFFER;
  sp->u.buf = b;
}

void
push_refed_buffer (buffer_t * b)
{
  sp++;
  sp->type = T_BUFFER;
  sp->u.buf = b;
}

/*
 * Push a mapping on the stack.  See push_array(), above.
 */
void
push_mapping (mapping_t * m)
{
  m->ref++;
  sp++;
  sp->type = T_MAPPING;
  sp->u.map = m;
}

void
push_refed_mapping (mapping_t * m)
{
  sp++;
  sp->type = T_MAPPING;
  sp->u.map = m;
}

/*
 * Push a class on the stack.  See push_array(), above.
 */
void
push_class (array_t * v)
{
  v->ref++;
  sp++;
  sp->type = T_CLASS;
  sp->u.arr = v;
}

void
push_refed_class (array_t * v)
{
  sp++;
  sp->type = T_CLASS;
  sp->u.arr = v;
}

/*
 * Push a string on the stack that is already malloced.
 */
void
push_malloced_string (char *p)
{
  sp++;
  sp->type = T_STRING;
  sp->u.string = p;
  sp->subtype = STRING_MALLOC;
}

/*
 * Pushes a known shared string.  Note that this references, while 
 * push_malloced_string doesn't.
 */
void
push_shared_string (char *p)
{
  sp++;
  sp->type = T_STRING;
  sp->u.string = p;
  sp->subtype = STRING_SHARED;
  ref_string (p);
}

/*
 * Push a string on the stack that is already constant.
 */
void push_constant_string (const char *p) {
  CHECK_AND_PUSH(1);
  sp->type = T_STRING;
  sp->subtype = STRING_CONSTANT;
  sp->u.const_string = p;
}

/*
 * Pop the top-most value of the stack.
 * Don't do this if it is a value that will be used afterwards, as the
  * data may be sent to FREE(), and destroyed.
  */
void pop_stack () {
  DEBUG_CHECK (sp < start_of_stack, "Stack underflow.\n");
  free_svalue (sp--, "pop_stack");
}

void pop_n_elems (size_t n) {
  while (n--)
    {
      pop_stack ();
    }
}

void pop_2_elems () {
  free_svalue (sp--, "pop_2_elems");
  DEBUG_CHECK (sp < start_of_stack, "Stack underflow.\n");
  free_svalue (sp--, "pop_2_elems");
}

void pop_3_elems () {
  free_svalue (sp--, "pop_3_elems");
  free_svalue (sp--, "pop_3_elems");
  DEBUG_CHECK (sp < start_of_stack, "Stack underflow.\n");
  free_svalue (sp--, "pop_3_elems");
}

/**
 * When an object is destructed, all references to it must be removed from the stack.
 * This function scans the stack for references to the object and replaces them with 0.
 * @param ob the object being destructed
 */
void remove_object_from_stack (object_t * ob) {
  svalue_t *svp;

  for (svp = start_of_stack; svp <= sp; svp++)
    {
      if (svp->type != T_OBJECT)
        continue;
      if (svp->u.ob != ob)
        continue;
      free_object (svp->u.ob, "remove_object_from_stack");
      svp->type = T_NUMBER;
      svp->u.number = 0;
    }
}
