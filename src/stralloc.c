/*  $Id: stralloc.c,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "LPC/runtime_config.h"
#include "rc.h"
#include "interpret.h"
#include "stralloc.h"
#include "hash.h"

#define NEXT(x)		(x)->next
#define REFS(x)		(x)->refs
#define EXTRA_REF(x)	(x)->extra_ref
#define SIZE(x)		(x)->size
#define BLOCK(x)	(((block_t *)(x)) - 1)	/* pointer arithmetic */
#define STRING(x)	((char *)(x + 1))

/* used temporarily by SVALUE_STRLEN() */
int svalue_strlen_size;

/*
   this code is not the same as the original code.  I cleaned it up to
   use structs to: 1) make it easier to check the driver for memory leaks
   (using MallocDebug on a NeXT) and 2) because it looks cleaner this way.
   --Truilkan@TMI 92/04/19

   modified to make calls to strlen() unnecessary and to remove superfluous
   calls to findblock().  -- Truilkan@TMI, 1992/08/05
*/

/*
 * stralloc.c - string management.
 *
 * All strings are stored in an extensible hash table, with reference counts.
 * free_string decreases the reference count; if it gets to zero, the string
 * will be deallocated.  add_string increases the ref count if it finds a
 * matching string, or allocates it if it cant.  There is no way to allocate
 * a string of a particular size to fill later (hash wont work!), so you'll
 * have to copy things from a static (or malloced and later freed) buffer -
 * that is, if you want to avoid space leaks...
 *
 * Current overhead:
 *	sizeof(block_t) per string (string pointer, next pointer, and a short
 *  for refs). Strings are nearly all fairly short, so this is a significant
 *  overhead - there is also the 4 byte malloc overhead and the fact that
 *  malloc generally allocates blocks which are a power of 2 (should write my
 *	own best-fit malloc specialised to strings); then again, GNU malloc
 *	is bug free...
 */

/*
 * there is also generic hash table management code, but strings can be shared
 * (that was the point of this code), will be unique in the table,
 * require a reference count, and are malloced, copied and freed at
 * will by the string manager.  Besides, I wrote this code first :-).
 * Look at htable.c for the other code.  It uses the Hash() function
 * defined here, and requires hashed objects to have a pointer to the
 * next element in the chain (which you specify when you call the functions).
 */

#ifdef STRING_STATS
int num_distinct_strings = 0;
int bytes_distinct_strings = 0;
int overhead_bytes = 0;
int allocd_strings = 0;
int allocd_bytes = 0;
int search_len = 0;
int num_str_searches = 0;
#endif

#define StrHash(s) (whashstr((s), 20) & (htable_size_minus_one))

#define hfindblock(s, h) sfindblock(s, h = StrHash(s))
#define findblock(s) sfindblock(s, StrHash(s))

static inline block_t *sfindblock (char *, int);

/*
 * hash table - list of pointers to heads of string chains.
 * Each string in chain has a pointer to the next string and a
 * reference count (char *, int) stored just before the start of the string.
 * HTABLE_SIZE is in config.h, and should be a prime, probably between
 * 1000 and 5000.
 */

static block_t **base_table = (block_t **) 0;
static int htable_size;
static int htable_size_minus_one;

static inline block_t *alloc_new_string (char *, int);

void
init_strings ()
{
  int x, y;

  /* ensure that htable size is a power of 2 */
  y = CONFIG_INT (__SHARED_STRING_HASH_TABLE_SIZE__);
  for (htable_size = 1; htable_size < y; htable_size *= 2)
    ;
  htable_size_minus_one = htable_size - 1;
  base_table = CALLOCATE (htable_size, block_t *,
			  TAG_STR_TBL, "init_strings");
#ifdef STRING_STATS
  overhead_bytes += (sizeof (block_t *) * htable_size);
#endif

  for (x = 0; x < htable_size; x++)
    {
      base_table[x] = 0;
    }

  //debug_trace ("sizeof malloc_block_t = %d", sizeof(malloc_block_t));
  //debug_trace ("sizeof block_t = %d", sizeof(block_t));
}

/*
 * Looks for a string in the table.  If it finds it, returns a pointer to
 * the start of the string part, and moves the entry for the string to
 * the head of the pointer chain.  One thing (blech!) - puts the previous
 * pointer on the hash chain into fs_prev.
 */

static inline block_t *
sfindblock (char *s, int h)
{
  block_t *curr, *prev;

  curr = base_table[h];
  prev = NULL;
#ifdef STRING_STATS
  num_str_searches++;
#endif

  while (curr)
    {
#ifdef STRING_STATS
      search_len++;
#endif
      if (*(STRING (curr)) == *s && !strcmp (STRING (curr), s))
	{			/* found it */
	  if (prev)
	    {			/* not at head of list */
	      NEXT (prev) = NEXT (curr);
	      NEXT (curr) = base_table[h];
	      base_table[h] = curr;
	    }
	  return (curr);	/* pointer to string */
	}
      prev = curr;
      curr = NEXT (curr);
    }
  return ((block_t *) 0);	/* not found */
}

char *
findstring (char *s)
{
  block_t *b;

  if ((b = findblock (s)))
    {
      return STRING (b);
    }
  else
    {
      return (NULL);
    }
}

/* alloc_new_string: Make a space for a string.  */

static inline block_t *
alloc_new_string (char *string, int h)
{
  block_t *b;
  int len = strlen (string);
  int size;

  if (len > CONFIG_INT (__MAX_STRING_LENGTH__))
    {
      len = CONFIG_INT (__MAX_STRING_LENGTH__);
    }
  size = sizeof (block_t) + len + 1;
  b = (block_t *) DXALLOC (size, TAG_SHARED_STRING, "alloc_new_string");
  strncpy (STRING (b), string, len);
  STRING (b)[len] = '\0';	/* strncpy doesn't put on \0 if 'from' too
				 * long */
  SIZE (b) = (len > USHRT_MAX ? USHRT_MAX : len);
  REFS (b) = 1;
  NEXT (b) = base_table[h];
  base_table[h] = b;
  ADD_NEW_STRING (SIZE (b), sizeof (block_t));
  ADD_STRING (SIZE (b));

  return (b);
}

char *
make_shared_string (char *str)
{
  block_t *b;
  int h;

  b = hfindblock (str, h);	/* hfindblock macro sets h = StrHash(s) */
  if (!b)
    {
      b = alloc_new_string (str, h);
    }
  else
    {
      if (REFS (b))
	REFS (b)++;
      ADD_STRING (SIZE (b));
    }
  return (STRING (b));
}

/*
   ref_string: Fatal to call this function on a string that isn't shared.
*/

char *
ref_string (char *str)
{
  block_t *b;

  b = BLOCK (str);
#ifdef DEBUG
  if (b != findblock (str))
    {
      fatal ("stralloc.c: called ref_string on non-shared string: %s.\n",
	     str);
    }
#endif /* defined(DEBUG) */
  if (REFS (b))
    REFS (b)++;
  ADD_STRING (SIZE (b));
  return str;
}

/* free_string: fatal to call free_string on a non-shared string */
/*
 * free_string - reduce the ref count on a string.  Various sanity
 * checks applied.
 */

void
free_string (char *str)
{
  block_t **prev, *b;
  int h;

  b = BLOCK (str);
  DEBUG_CHECK1 (b != findblock (str),
		"stralloc.c: free_string called on non-shared string: %s.\n",
		str);

  /*
   * if a string has been ref'd USHRT_MAX times then we assume that its used
   * often enough to justify never freeing it.
   */
  if (!REFS (b))
    return;

  REFS (b)--;
  SUB_STRING (SIZE (b));

  if (REFS (b) > 0)
    return;

  h = StrHash (str);
  prev = base_table + h;
  while ((b = *prev))
    {
      if (STRING (b) == str)
	{
	  *prev = NEXT (b);
	  break;
	}
      prev = &(NEXT (b));
    }

  SUB_NEW_STRING (SIZE (b), sizeof (block_t));
  FREE (b);
}

void
deallocate_string (char *str)
{
  int h;
  block_t *b, **prev;

  h = StrHash (str);
  prev = base_table + h;
  while ((b = *prev))
    {
      if (STRING (b) == str)
	{
	  *prev = NEXT (b);
	  break;
	}
      prev = &(NEXT (b));
    }
  DEBUG_CHECK1 (!b,
		"stralloc.c: deallocate_string called on non-shared string: %s.\n",
		str);

  FREE (b);
}

int
add_string_status (outbuffer_t * out, int verbose)
{
#ifdef STRING_STATS
  if (verbose == 1)
    {
      outbuf_add (out, "All strings:\n");
      outbuf_add (out, "-------------------------\t Strings    Bytes\n");
    }
  if (verbose != -1)
    outbuf_addv (out, "All strings:\t\t\t%7d %8d + %d overhead\n",
		 num_distinct_strings, bytes_distinct_strings,
		 overhead_bytes);
  if (verbose == 1)
    {
      outbuf_addv (out, "Total asked for\t\t\t%8d %8d\n",
		   allocd_strings, allocd_bytes);
      outbuf_addv (out, "Space actually required/total string bytes %d%%\n",
		   (bytes_distinct_strings +
		    overhead_bytes) * 100 / allocd_bytes);
      outbuf_addv (out, "Searches: %d    Average search length: %6.3f\n",
		   num_str_searches, (double) search_len / num_str_searches);
    }
  return (bytes_distinct_strings + overhead_bytes);
#else
  if (verbose)
    outbuf_add (out,
		"<String statistics disabled, no information available>\n");
  return 0;
#endif
}

#define DME

/* This stuff needs a bit more work, otherwise FREE_MSTR() will crash on this
malloc_block_t the_null_string_blocks[2] = { { DME 0, 1 }, { DME 0, 0 } };

char *the_null_string = (char *)&the_null_string_blocks[1];
*/

char *
int_new_string (int size)
{
  malloc_block_t *mbt;

  mbt =
    (malloc_block_t *) DXALLOC (size + sizeof (malloc_block_t) + 1,
				TAG_MALLOC_STRING, tag);
  if (size < USHRT_MAX)
    {
      mbt->size = size;
      ADD_NEW_STRING (size, sizeof (malloc_block_t));
    }
  else
    {
      mbt->size = USHRT_MAX;
      ADD_NEW_STRING (USHRT_MAX, sizeof (malloc_block_t));
    }
  mbt->ref = 1;
  ADD_STRING (mbt->size);
  return (char *) (mbt + 1);
}

char *
extend_string (char *str, int len)
{
  malloc_block_t *mbt;
#ifdef STRING_STATS
  int oldsize = MSTR_SIZE (str);
#endif

  mbt =
    (malloc_block_t *) DREALLOC (MSTR_BLOCK (str),
				 len + sizeof (malloc_block_t) + 1,
				 TAG_MALLOC_STRING, "extend_string");
  if (len < USHRT_MAX)
    {
      mbt->size = len;
    }
  else
    {
      mbt->size = USHRT_MAX;
    }
  ADD_STRING_SIZE (mbt->size - oldsize);

  return (char *) (mbt + 1);
}

char *
int_alloc_cstring (char *str)
{
  char *ret;

  ret = (char *) DXALLOC (strlen (str) + 1, TAG_STRING, tag);
  strcpy (ret, str);
  return ret;
}

char *
int_string_copy (char *str)
{
  char *p;
  int len;

  DEBUG_CHECK (!str, "Null string passed to string_copy.\n");
  len = strlen (str);
  if (len > CONFIG_INT (__MAX_STRING_LENGTH__))
    {
      len = CONFIG_INT (__MAX_STRING_LENGTH__);
      p = new_string (len, desc);
      (void) strncpy (p, str, len);
      p[len] = '\0';
    }
  else
    {
      p = new_string (len, desc);
      (void) strncpy (p, str, len + 1);
    }
  return p;
}

char *
int_string_unlink (char *str)
{
  malloc_block_t *mbt, *newmbt;

  mbt = ((malloc_block_t *) str) - 1;
  mbt->ref--;

  if (mbt->size == USHRT_MAX)
    {
      int l = strlen (str + USHRT_MAX) + USHRT_MAX;	/* ouch */

      newmbt =
	(malloc_block_t *) DXALLOC (l + sizeof (malloc_block_t) + 1,
				    TAG_MALLOC_STRING, desc);
      memcpy ((char *) (newmbt + 1), (char *) (mbt + 1), l + 1);
      newmbt->size = USHRT_MAX;
      ADD_NEW_STRING (USHRT_MAX, sizeof (malloc_block_t));
    }
  else
    {
      newmbt =
	(malloc_block_t *) DXALLOC (mbt->size + sizeof (malloc_block_t) + 1,
				    TAG_MALLOC_STRING, desc);
      memcpy ((char *) (newmbt + 1), (char *) (mbt + 1), mbt->size + 1);
      newmbt->size = mbt->size;
      ADD_NEW_STRING (mbt->size, sizeof (malloc_block_t));
    }
  newmbt->ref = 1;

  return (char *) (newmbt + 1);
}

inline void
free_string_svalue (svalue_t * v)
{
  char *str = v->u.string;

  if (v->subtype & STRING_COUNTED)
    {
#ifdef STRING_STATS
      int size = MSTR_SIZE (str);
#endif
      if (DEC_COUNTED_REF (str))
	{
	  SUB_STRING (size);
	  if (v->subtype & STRING_HASHED)
	    {
	      SUB_NEW_STRING (size, sizeof (block_t));
	      deallocate_string (str);
	    }
	  else
	    {
	      SUB_NEW_STRING (size, sizeof (malloc_block_t));
	      FREE (MSTR_BLOCK (str));
	    }
	}
      else
	{
	  SUB_STRING (size);
	}
    }
}

void
unlink_string_svalue (svalue_t * s)
{
  char *str;

  switch (s->subtype)
    {
    case STRING_MALLOC:
      if (MSTR_REF (s->u.string) > 1)
	s->u.string = string_unlink (s->u.string, "unlink_string_svalue");
      break;
    case STRING_SHARED:
      {
	int l = SHARED_STRLEN (s->u.string);

	str = new_string (l, "unlink_string_svalue");
	strncpy (str, s->u.string, l + 1);
	free_string (s->u.string);
	s->subtype = STRING_MALLOC;
	s->u.string = str;
	break;
      }
    case STRING_CONSTANT:
      s->u.string = string_copy (sp->u.string, "unlink_string_svalue");
      s->subtype = STRING_MALLOC;
      break;
    }
}
