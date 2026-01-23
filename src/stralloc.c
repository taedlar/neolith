#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "lpc/include/runtime_config.h"
#include "interpret.h"
#include "stralloc.h"
#include "hash.h"

#include <assert.h>

/* block_t only */
#define NEXT(x)		(x)->next
#define REFS(x)		(x)->refs
#define BLOCK(x)	(((block_t *)(x)) - 1)	/* pointer arithmetic */

/* block_t or malloc_block_t */
#define SIZE(x)		(x)->size
#define STRING(x)	((char *)(x + 1))

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
size_t bytes_distinct_strings = 0;
size_t overhead_bytes = 0;
int allocd_strings = 0;
size_t allocd_bytes = 0;
int search_len = 0;
int num_str_searches = 0;
#endif

#define StrHash(s) (whashstr((s), 20) & (htable_size_minus_one))

#define hfindblock(s, h) sfindblock(s, h = StrHash(s))
#define findblock(s) sfindblock(s, StrHash(s))

static block_t *sfindblock (const char *s, int h);

/*
 * hash table - list of pointers to heads of string chains.
 * Each string in chain has a pointer to the next string and a
 * reference count (char *, int) stored just before the start of the string.
 * HTABLE_SIZE is in config.h, and should be a prime, probably between
 * 1000 and 5000.
 */

static block_t **base_table = (block_t **) 0;
static size_t htable_size;
static size_t htable_size_minus_one;
static size_t max_string_length;

static block_t *sfindblock (const char *s, int h);
static void dealloc_string (const char *str);
static block_t* alloc_new_string (const char *, int);

/**
 * @brief init_strings: Initialize the shared string table.
 */
void init_strings (size_t hash_size, size_t max_len) {

  size_t x;

  /* ensure that htable size is a power of 2 */
  for (htable_size = 1; htable_size < hash_size; htable_size *= 2)
    ;
  htable_size_minus_one = htable_size - 1;
  base_table = CALLOCATE (htable_size, block_t *, TAG_STR_TBL, "init_strings");
#ifdef STRING_STATS
  overhead_bytes += (sizeof (block_t *) * htable_size);
#endif

  for (x = 0; x < htable_size; x++)
    {
      base_table[x] = 0;
    }

  max_string_length = max_len;
}

void deinit_strings(void) {
  if (base_table)
    {
      if (num_distinct_strings > 0)
        debug_message ("Warning: deinit_strings with %d strings still allocated.\n", num_distinct_strings);
      /* TODO: free all strings */
      FREE (base_table);
      base_table = 0;
    }
}

/**
 * Looks for a string in the table.  If it finds it, returns a pointer to
 * the start of the string part, and moves the entry for the string to
 * the head of the pointer chain.
 */
static block_t *sfindblock (const char *s, int h) {

  block_t *curr, *prev;

  if (!base_table)
    fatal ("stralloc.c: stralloc used before init_strings()\n");
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
        {
          /* found it */
          if (prev)
            {
              /* Move the found block to the head of the list.
               * This improves performance for strings that are
               * looked up frequently.
               */
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

/**
 * Find a shared string in the string table or return NULL if not found.
 */
char* findstring (const char *s) {
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

/**
 * Make a space for a string and add it to the hash table.
 * This is the internal function of the string manager.
 * For external use, see make_shared_string().
 * 
 * @param string The string to add.
 * @param h The hash value of the string.
 * @return A pointer to the newly allocated string block entry.
 * @see make_shared_string
 */
static block_t* alloc_new_string (const char *string, int h) {

  block_t *b;
  size_t len = strlen (string);
  size_t size;

  opt_trace (TT_MEMORY|1, "first ref: \"%s\"", string);
  if (len > max_string_length)
    {
      len = max_string_length;
    }

  /* A shared string is allocated with a block_t header followed by
   * the string data itself:
   *  +-----------------+----------------+------+
   *  | block_t header  | string data... | '\0' |
   *  +-----------------+----------------+------+
   */
  size = sizeof (block_t) + len + 1;
  b = (block_t *) DXALLOC (size, TAG_SHARED_STRING, "alloc_new_string");
  strncpy (STRING (b), string, len);
  STRING (b)[len] = '\0';	/* truncate string if its length exceeds max_string_length */

  /* The 'size' field stores a 16-bit length of the string.
   * If the string length exceeds USHRT_MAX (0xffff), the 'size' is set to USHRT_MAX.
   * Note that the string data itself is always allocated with the full length regardless.
   */
  SIZE (b) = (unsigned short)(len > USHRT_MAX ? USHRT_MAX : len);
  REFS (b) = 1;
  /* add to string hash table */
  NEXT (b) = base_table[h];
  base_table[h] = b;
  /* update string stats */
  ADD_NEW_STRING (SIZE (b), sizeof (block_t));
  ADD_STRING (SIZE (b));

  return (b);
}

/**
 * Create or retrieve a shared string (reference counted).
 * - If the string already exists in the string table, its reference count
 *   is incremented and a pointer to the existing string is returned.
 * - If the string does not exist, a new entry is created in the string table
 *   and a pointer to the new string (with reference count 1) is returned.
 * @param str The string to share.
 * @return A pointer to the shared string.
 */
char* make_shared_string (const char *str) {
  block_t *b;
  int h;

  assert(str != NULL);

  b = hfindblock (str, h);	/* hfindblock macro sets h = StrHash(s) */
  if (!b)
    {
      b = alloc_new_string (str, h);
    }
  else
    {
      if (REFS (b)) /* if reference count overflown, let it stay zero ... */
        {
          opt_trace (TT_MEMORY|2, "add ref (was %d): \"%s\"", REFS (b), str);
          REFS (b)++;
        }
      ADD_STRING (SIZE (b));
    }

  /* Return a pointer to the string data. The block_t header is hidden from the caller. */
  return (STRING (b));
}

/**
 * Increase the reference count of a shared string.
 * It is fatal to call this function on a string that isn't shared.
 */
char* ref_string (char *str) {
  block_t *b;

  assert (str != NULL);
  b = BLOCK (str);
  assert (b == findblock (str)); /* ensure it's a shared string */

  if (REFS (b)) /* if reference count overflown, let it stay zero ... */
    {
      opt_trace (TT_MEMORY|2, "add ref (was %d): \"%s\"", REFS (b), str);
      REFS (b)++;
    }
  ADD_STRING (SIZE (b));
  return str;
}

/**
 * Reduce the reference count on a string.
 * Various sanity checks applied.
 * It's fatal to call this function on a non-shared string.
 * 
 * If the reference count goes to zero, the string is removed from the
 * hash table and the memory is freed.
 */
void free_string (char *str) {
  block_t **prev, *b;
  int h;

  assert (str != NULL);
  b = BLOCK (str);
  assert (b == findblock (str)); /* ensure it's a shared string */

  /*
   * if a string has been ref'd USHRT_MAX times then we assume that its used
   * often enough to justify never freeing it.
   */
  if (!REFS (b)) {
    opt_warn (2, "string \"%s\" has ref count 0, could be overflow", str);
    return;
  }

  opt_trace (TT_MEMORY|2, "release ref (was %d): \"%s\"", REFS (b), str);
  SUB_STRING (SIZE (b));
  if (--REFS (b) > 0)
    return;

  /* remove from hash table */
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

  /* free the shared string */
  SUB_NEW_STRING (SIZE (b), sizeof (block_t));
  opt_trace (TT_MEMORY|1, "dealloc: \"%s\"", str);
  FREE (b);
}

/**
 * Deallocate a shared string in the string hash table.
 * This function ignores the reference count and locate the string in the hash table
 * by the looking for the string data memory address directly.
 * If the string is not found, this is no-op.
 * 
 * @param str Pointer to the string to deallocate.
 */
static void dealloc_string (const char *str) {

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
  if (b)
    FREE (b);
}

size_t add_string_status (outbuffer_t * out, int verbose) {
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
    outbuf_add (out, "<String statistics disabled, no information available>\n");
  return 0;
#endif
}

/**
 * Create a new reference counted string (STRING_MALLOC) of specified size.
 * @param size The size of the string to allocate.
 * @return A pointer to the newly allocated string (uninitialized).
 */
char* int_new_string (size_t size) {
  malloc_block_t *mbt;

  mbt = (malloc_block_t *) DXALLOC (size + sizeof (malloc_block_t) + 1, TAG_MALLOC_STRING, tag);
  if (size < USHRT_MAX)
    {
      mbt->size = (unsigned short)size;
      ADD_NEW_STRING (size, sizeof (malloc_block_t));
    }
  else
    {
      mbt->size = USHRT_MAX;
      ADD_NEW_STRING (USHRT_MAX, sizeof (malloc_block_t)); /* FIXME: this is probably incorrect ... */
    }
  mbt->ref = 1;
  ADD_STRING (mbt->size);
  return STRING(mbt);
}

/**
 * Create a copy of a string as a reference counted string (STRING_MALLOC).
 * If the string length exceeds max_string_length, it is truncated.
 * @param str The string to copy. This can be any null-terminated string.
 * @return A pointer to the newly allocated string.
 */
char *int_string_copy (const char *str) {
  char *p;
  size_t len;

  assert (str != NULL);
  len = strlen (str);
  if (len > max_string_length)
    {
      len = max_string_length;
      p = new_string (len, desc);
      (void) strncpy (p, str, len); /* truncate */
      p[len] = '\0';
    }
  else
    {
      p = new_string (len, desc);
      (void) strncpy (p, str, len + 1); /* copy including null byte */
    }
  return p;
}

/**
 * Extend a reference counted string (STRING_MALLOC).
 */
char *extend_string (char *str, size_t len) {
  malloc_block_t *mbt;

  assert (str != NULL);
#ifdef STRING_STATS
  int oldsize = MSTR_SIZE (str);
#endif
  mbt = (malloc_block_t *) DREALLOC (MSTR_BLOCK (str), len + sizeof (malloc_block_t) + 1, TAG_MALLOC_STRING, "extend_string");
  if (len < USHRT_MAX)
    {
      mbt->size = (unsigned short)len;
    }
  else
    {
      mbt->size = USHRT_MAX;
    }
  ADD_STRING_SIZE (mbt->size - oldsize);
  return STRING(mbt);
}

/**
 * Allocate a new C string and copy the given string into it.
 * This function does exactly what strdup() does but uses the
 * driver memory allocation functions. No block header is added.
 * @param str The string to copy. This can be any null-terminated string.
 * @return A pointer to the newly allocated string.
 */
char *int_alloc_cstring (const char *str) {
  char *ret;

  assert (str != NULL);
  ret = (char *) DXALLOC (strlen (str) + 1, TAG_STRING, tag);
  strcpy (ret, str);
  return ret;
}

/**
 * Unlink a reference counted string (STRING_MALLOC or STRING_SHARED) by creating a new copy
 * of it as a STRING_MALLOC string.
 * The reference count of the original string is decremented.
 * @param str The string to unlink. This must be a STRING_MALLOC string with reference count > 1.
 * @return A pointer to the newly allocated string.
 */
static char *int_string_unlink (char *str) {
  malloc_block_t *newmbt;

  assert (str != NULL);
  assert (MSTR_REF (str) > 1);
  MSTR_REF (str)--; /* decrement reference count */

  if (MSTR_SIZE (str) == USHRT_MAX)
    {
      size_t len = strlen (str + USHRT_MAX) + USHRT_MAX;	/* ouch */

      newmbt = (malloc_block_t *) DXALLOC (len + sizeof (malloc_block_t) + 1, TAG_MALLOC_STRING, "int_string_unlink");
      memcpy (STRING(newmbt), str, len + 1);
      newmbt->size = USHRT_MAX;
      ADD_NEW_STRING (USHRT_MAX, sizeof (malloc_block_t)); /* FIXME: this is probably incorrect ... */
    }
  else
    {
      newmbt = (malloc_block_t *) DXALLOC (MSTR_SIZE (str) + sizeof (malloc_block_t) + 1, TAG_MALLOC_STRING, "int_string_unlink");
      memcpy (STRING(newmbt), str, MSTR_SIZE (str) + 1);
      newmbt->size = MSTR_SIZE (str);
      ADD_NEW_STRING (MSTR_SIZE (str), sizeof (malloc_block_t));
    }
  newmbt->ref = 1;
  return STRING(newmbt);
}

/**
 * Free a string svalue.
 * If the string is reference counted (STRING_MALLOC or STRING_SHARED),
 * decrease the reference count and free the string if the count reaches zero.
 * If the string is not reference counted (string constant), do nothing.
 * @param v Pointer to the string svalue to free.
 */
void free_string_svalue (svalue_t * v) {

  char *str = v->u.string;

  if (v->subtype & STRING_COUNTED) /* refence counted string? (STRING_MALLOC or STRING_SHARED) */
    {
#ifdef STRING_STATS
      int size = MSTR_SIZE (str);
#endif
      if (DEC_COUNTED_REF (str))
        {
          SUB_STRING (size);
          if (v->subtype & STRING_HASHED) /* STRING_SHARED */
            {
              SUB_NEW_STRING (size, sizeof (block_t));
              dealloc_string (str);
            }
          else /* STRING_MALLOC */
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

void unlink_string_svalue (svalue_t * s) {

  char *str;

  switch (s->subtype)
    {
    case STRING_MALLOC:
      if (MSTR_REF (s->u.string) > 1)
        s->u.string = int_string_unlink (s->u.string, "unlink_string_svalue");
      break;
    case STRING_SHARED:
      {
        size_t len = SHARED_STRLEN (s->u.string);

        str = new_string (len, "unlink_string_svalue");
        strncpy (str, s->u.string, len + 1);
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
