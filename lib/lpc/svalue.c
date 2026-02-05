#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <assert.h>
#include "src/std.h"
#include "buffer.h"
#include "svalue.h"

extern void dealloc_object (object_t *, const char* caller);
extern void dealloc_array (array_t *);
extern void dealloc_mapping (mapping_t *);
extern void dealloc_class (array_t *);
extern void dealloc_funp (funptr_t *);

/**
 * Free the data that an svalue is pointing to. Not the svalue itself.
 * The caller argument is used for debugging purposes, to know which function is freeing the svalue.
 */
void free_svalue (svalue_t * v, const char* caller) {

  assert (v != NULL);

  if (v->type == T_STRING)
    {
      free_string_svalue (v);
    }
  else if (v->type & T_REFED)
    {
      if (!(--v->u.refed->ref))
        {
          switch (v->type)
            {
            case T_OBJECT:
              dealloc_object (v->u.ob, caller);
              break;
            case T_CLASS:
              dealloc_class (v->u.arr);
              break;
            case T_ARRAY:
              dealloc_array (v->u.arr);
              break;
            case T_BUFFER:
              if (v->u.buf != &null_buf)
                FREE ((char *) v->u.buf);
              break;
            case T_MAPPING:
              dealloc_mapping (v->u.map);
              break;
            case T_FUNCTION:
              dealloc_funp (v->u.fp);
              break;
            }
        }
    }
  else if (v->type == T_ERROR_HANDLER)
    {
      (*v->u.error_handler) ();
    }
}

/**
 * Free several svalues, and free up the space used by the svalues.
 * The svalues must be sequentially located.
 */
void free_some_svalues (svalue_t * v, int num) {
  while (num--)
    free_svalue (v + num, "free_some_svalues");
  FREE (v);
}

void assign_svalue (svalue_t * dest, svalue_t * v) {
  /* First deallocate the previous value. */
  free_svalue (dest, "assign_svalue");
  assign_svalue_no_free (dest, v);
}

/**
 * Assign to a svalue.
 * This is done either when element in array, or when to an identifier
 * (as all identifiers are kept in a array pointed to by the object).
 */
void assign_svalue_no_free (svalue_t * to, svalue_t * from) {
  DEBUG_CHECK (from == 0, "Attempt to assign_svalue() from a null ptr.\n");
  DEBUG_CHECK (to == 0, "Attempt to assign_svalue() to a null ptr.\n");
  *to = *from;

  if (from->type == T_STRING)
    {
      if (from->subtype & STRING_COUNTED)
        {
          INC_COUNTED_REF (to->u.string);
/*	    ADD_STRING(MSTR_SIZE(to->u.string)); */
        }
    }
  else if (from->type & T_REFED)
    {
      from->u.refed->ref++;
    }
}

/*
 * Copies an array of svalues to another location, which should be
 * free space.
 */
void copy_some_svalues (svalue_t * dest, svalue_t * v, int num) {
  while (num--)
    assign_svalue_no_free (dest + num, v + num);
}
