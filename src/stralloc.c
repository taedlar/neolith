#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "lpc/include/runtime_config.h"
#include "interpret.h"
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

  modernized further to support length-aware shared-string lookup via
  explicit spans (start/end) while preserving NUL-terminated call paths.
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

#define StrHashN(s, len) (whashstr((s), (s) + (len), 20) & (htable_size_minus_one))

#define hfindblockn(s, len, h) sfindblock(s, len, h = StrHashN(s, len))
#define findblockn(s, len) sfindblock(s, len, StrHashN(s, len))

static block_t *sfindblock (const char *s, size_t len, int h);

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

static block_t *sfindblock (const char *s, size_t len, int h);
static void dealloc_string (shared_str_t str);
static block_t* alloc_new_string (const char *, size_t, int);

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
  size_t i, s = 0;
  if (base_table)
    {
      /* dump all strings */
      for (i = 0; i < htable_size; i++)
        {
          block_t *b, *next;

          b = base_table[i];
          while (b)
            {
              next = NEXT (b);
              if (REFS (b) > 0)
                {
                  opt_trace (TT_MEMORY|1, "leaked (ref=%d): @%p", REFS (b), (void *)STRING (b));
                  s++;
                }
              else
                num_distinct_strings--; /* immortal strings, we free them here */
              FREE (b);
              b = next;
            }
        }
      if (s)
        debug_warn ("%d shared strings still allocated.\n", s);
      FREE (base_table);
      base_table = 0;
    }
#ifdef STRING_STATS
  if (num_distinct_strings > 0)
    debug_warn ("%d reference counting strings still allocated.\n", num_distinct_strings);
#endif
}

/**
 * Looks for a byte span in a hash bucket.
 * If found, returns the owning block and moves it to the head of the chain.
 * Lookup is length-aware (SIZE + memcmp), so embedded NUL bytes are handled.
 */
static block_t *sfindblock (const char *s, size_t len, int h) {

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
      if (SIZE (curr) == len && memcmp (STRING (curr), s, len) == 0)
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
 * Find a shared string by key.
 * If end is non-NULL, key bytes are [s, end). Otherwise s is treated as a
 * NUL-terminated C string. Returns NULL when not found.
 */
shared_str_t findstring (const char *s, const char *end) {
  block_t *b;
  size_t len;

  assert (s != NULL);
  if (end && end <= s)
    return NULL;
  len = end ? (size_t)(end - s) : strlen (s);
  b = findblockn (s, len);
  return b ? STRING (b) : NULL;
}

/**
 * Allocate and insert a new shared-string block.
 * The payload length is provided by the caller and may include embedded NUL.
 * A trailing '\0' guard is still appended for compatibility with legacy paths.
 *
 * @param string Pointer to source bytes.
 * @param len Number of payload bytes.
 * @param h Precomputed hash bucket index.
 * @return Pointer to the newly allocated block.
 */
static block_t* alloc_new_string (const char *string, size_t len, int h) {

  block_t *b;
  size_t size;

  opt_trace (TT_MEMORY|2, "first ref @%p, len=%zu", (void *)string, len);

  /* A shared string is allocated with a block_t header followed by
   * the string data itself:
   *  +-----------------+----------------+------+
   *  | block_t header  | string data... | '\0' |
   *  +-----------------+----------------+------+
   */
  size = sizeof (block_t) + len + 1;
  b = (block_t *) DXALLOC (size, TAG_SHARED_STRING, "alloc_new_string");
  memcpy (STRING (b), string, len);
  STRING (b)[len] = '\0';	/* NUL guard; not part of the logical string */

  /* Shared strings are capped below USHRT_MAX, so 'size' is exact. */
  SIZE (b) = (unsigned short)len;
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
 * - If end is non-NULL, bytes in [str, end) are interned (span mode).
 * - Otherwise str is interpreted as a NUL-terminated C string.
 * - Oversize inputs are truncated to the maximum shared-string length.
 *
 * Existing entry: increments refcount and returns existing payload pointer.
 * New entry: inserts a new shared block and returns its payload pointer.
 */
shared_str_t make_shared_string (const char *str, const char *end) {
  block_t *b;
  int h;
  size_t hard_limit;
  size_t effective_len;

  assert(str != NULL);

  hard_limit = max_string_length;
  if (hard_limit >= USHRT_MAX)
    {
      hard_limit = USHRT_MAX - 1;
    }

  if (end)
    {
      if (end <= str)
        {
          return make_shared_string ("", NULL);
        }
      effective_len = (size_t)(end - str);
      if (effective_len > hard_limit)
        {
          effective_len = hard_limit;
        }
    }
  else
    {
      /*
       * Probe only up to the maximum representable shared-string length.
       * If no NUL is found in that window, the input must be truncated.
       */
      const char *nul = (const char *)memchr (str, '\0', hard_limit + 1);
      effective_len = nul ? (size_t)(nul - str) : hard_limit;
    }

  b = hfindblockn (str, effective_len, h);
  if (!b)
    {
      b = alloc_new_string (str, effective_len, h);
    }
  else
    {
      if (REFS (b)) /* if reference count overflown, let it stay zero ... */
        {
          opt_trace (TT_MEMORY|3, "add ref (was %d): @%p", REFS (b), (void *)str);
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
shared_str_t ref_string (shared_str_t str) {
  block_t *b;

  assert (str != NULL);
  b = BLOCK (str);
  assert (b == findblockn (str, SIZE (b))); /* ensure it's a shared string */

  if (REFS (b)) /* if reference count overflown, let it stay zero ... */
    {
      opt_trace (TT_MEMORY|3, "add ref (was %d): @%p", REFS (b), (void *)str);
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
void free_string (shared_str_t str) {
  block_t **prev, *b;
  int h;

  assert (str != NULL);
  b = BLOCK (str);
  assert (b == findblockn (str, SIZE (b))); /* ensure it's a shared string */

  /*
   * if a string has been ref'd USHRT_MAX times then we assume that its used
   * often enough to justify never freeing it.
   */
  if (!REFS (b)) {
    opt_warn (2, "string @%p has ref count 0, could be overflow", (void *)str);
    return;
  }

  opt_trace (TT_MEMORY|3, "release ref (was %d): @%p", REFS (b), (void *)str);
  SUB_STRING (SIZE (b));
  if (--REFS (b) > 0)
    return;

  /* remove from hash table */
  h = StrHashN (str, SIZE (b));
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
  opt_trace (TT_MEMORY|2, "dealloc: @%p", (void *)str);
  FREE (b);
}

/**
 * Deallocate a shared string from the hash table, ignoring reference count.
 *
 * This helper is used by free_string_svalue() when a counted STRING_SHARED
 * reaches refcount zero through generic counted-string macros.
 * The hash bucket is derived from the shared block header length, then the
 * entry is removed by payload-pointer identity.
 *
 * If the pointer is not found in the expected bucket, this is a no-op.
 */
static void dealloc_string (shared_str_t str) {

  int h;
  block_t *b, **prev;
  size_t len;

  assert (str != NULL);
  len = SIZE (BLOCK (str));

  h = StrHashN (str, len);
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
 * @return A pointer to the newly allocated string. The payload bytes [0,size)
 *         are uninitialized, but byte [size] is always set to '\0' as a
 *         compatibility guard and is not part of the logical counted length.
 */
malloc_str_t int_new_string (size_t size) {
  malloc_block_t *mbt;

  mbt = (malloc_block_t *) DXALLOC (size + sizeof (malloc_block_t) + 1, TAG_MALLOC_STRING, tag);
  mbt->blkend = NULL;
  if (size < USHRT_MAX)
    {
      mbt->size = (unsigned short)size;
      ADD_NEW_STRING (size, sizeof (malloc_block_t));
    }
  else
    {
      mbt->size = USHRT_MAX;
      mbt->blkend = STRING(mbt) + size;
      ADD_NEW_STRING (USHRT_MAX, sizeof (malloc_block_t)); /* FIXME: this is probably incorrect ... */
    }
  mbt->ref = 1;
  ADD_STRING (mbt->size);
  STRING(mbt)[size] = '\0';
  return STRING(mbt);
}

/**
 * Create a copy of a string as a reference counted string (STRING_MALLOC).
 * If the string length exceeds max_string_length, it is truncated.
 * @param str The string to copy.
 * @param end If non-NULL, the string to copy is the byte span [str, end).
 *            Otherwise str is treated as a NUL-terminated C string.
 * @return A pointer to the newly allocated string.
 *         A NUL-terminator is appended for compatibility, but is not counted in the length.
 */
malloc_str_t int_string_copy (const char *str, const char *end) {
  malloc_str_t p;
  size_t len = 0;

  if (!str)
    fatal ("string_copy: null pointer passed as string argument");
  if (end)
    {
      if (end == str)
        len = 0;
      else if (end > str)
        len = (size_t)(end - str);
      else
        fatal ("string_copy: end pointer is before start pointer");
    }
  else
    len = strlen (str);

  if (len > max_string_length)
    {
      len = max_string_length;
      p = new_string (len, desc);
      memcpy (p, str, len); /* truncate */
      p[len] = '\0';
    }
  else
    {
      p = new_string (len, desc);
      memcpy (p, str, len);
      p[len] = '\0';
    }
  return p;
}

/**
 * Extend a reference counted string (STRING_MALLOC).
 * @param str The string to extend. This must be a STRING_MALLOC string.
 * @param len The new desired length of the string.
 *            NOTE: If this exceeds max_string_length, it is NOT truncated.
 * @return A pointer to the extended string. The original string pointer must not be used after this call.
 *         Byte [len] is always set to '\0' for compatibility, but is not
 *         counted in the logical length.
 */
malloc_str_t int_extend_string (malloc_str_t str, size_t len) {
  malloc_block_t *mbt;

  if (!str)
    fatal ("extend_string: null pointer passed as string argument");
#ifdef STRING_TYPE_SAFETY
  if (MSTR_REF (str) == 0)
    fatal ("extend_string: contract violation: ref count is 0 (immortal STRING_SHARED or freed pointer)\n");
#endif
#ifdef STRING_STATS
  int oldsize = MSTR_SIZE (str);
#endif
  mbt = (malloc_block_t *) DREALLOC (MSTR_BLOCK (str), len + sizeof (malloc_block_t) + 1, TAG_MALLOC_STRING, "extend_string");
  if (len < USHRT_MAX)
    {
      mbt->size = (unsigned short)len;
      mbt->blkend = NULL;
    }
  else
    {
      mbt->size = USHRT_MAX;
      mbt->blkend = STRING(mbt) + len;
    }
  ADD_STRING_SIZE (mbt->size - oldsize);
  STRING(mbt)[len] = '\0';
  return STRING(mbt);
}

/**
 * Allocate a new C string and copy the given string into it.
 * The returned string is always NUL-terminated, and the length is determined by either
 * the provided end pointer or the first NUL byte in str.
 * No block header is added.
 * @param str The string to copy. This can be any null-terminated string.
 * @param end If non-NULL, the string to copy is the byte span [str, end).
 *            Otherwise str is treated as a NUL-terminated C string.
 * @return A pointer to the newly allocated string.
 *         A NUL-terminator is appended for compatibility, but is not counted in the length.
 */
char *int_alloc_cstring (const char *str, const char *end) {
  char *ret;
  size_t len = 0;

  if (!str)
    fatal ("alloc_cstring: null pointer passed as string argument");
  if (end)
    {
      if (end == str)
        len = 0;
      else if (end > str)
        len = (size_t)(end - str);
      else
        fatal ("alloc_cstring: end pointer is before start pointer");
    }
  else
      len = strlen (str);

  ret = (char *) DXALLOC (len + 1, TAG_STRING, tag);
  memcpy (ret, str, len);
  ret[len] = '\0';
  return ret;
}

/**
 * Unlink a reference counted string (STRING_MALLOC or STRING_SHARED) by creating a new copy
 * of it as a STRING_MALLOC string.
 * The reference count of the original string is decremented.
 * @param str The string to unlink. This must be a STRING_MALLOC string with reference count > 1.
 * @return A pointer to the newly allocated string.
 */
malloc_str_t int_string_unlink (malloc_str_t str) {
  malloc_block_t *newmbt;

  assert (str != NULL);
  assert (MSTR_REF (str) > 1);
#ifdef STRING_TYPE_SAFETY
  if (MSTR_REF (str) <= 1) {
    debug_fatal ("string_unlink: contract violation: ref count not > 1 (not a live STRING_MALLOC with multiple refs)\n");
    abort ();
  }
#endif
  MSTR_REF (str)--; /* decrement reference count */

  if (MSTR_SIZE (str) == USHRT_MAX)
    {
      size_t len;
      if (MSTR_BLKEND (str))
        {
          len = (size_t)((char *)MSTR_BLKEND (str) - str);
        }
      else
        {
          len = strlen (str + USHRT_MAX) + USHRT_MAX;	/* fallback for old strings */
        }

      newmbt = (malloc_block_t *) DXALLOC (len + sizeof (malloc_block_t) + 1, TAG_MALLOC_STRING, "string_unlink");
      memcpy (STRING(newmbt), str, len + 1);
      newmbt->blkend = STRING(newmbt) + len;
      newmbt->size = USHRT_MAX;
      ADD_NEW_STRING (USHRT_MAX, sizeof (malloc_block_t)); /* FIXME: this is probably incorrect ... */
    }
  else
    {
      newmbt = (malloc_block_t *) DXALLOC (MSTR_SIZE (str) + sizeof (malloc_block_t) + 1, TAG_MALLOC_STRING, "string_unlink");
      memcpy (STRING(newmbt), str, MSTR_SIZE (str) + 1);
      newmbt->blkend = NULL;
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

  if (v->subtype & STRING_COUNTED) /* reference counted string? (STRING_MALLOC or STRING_SHARED) */
    {
      size_t size = MSTR_SIZE (str);
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

  malloc_str_t str;

  switch (s->subtype)
    {
    case STRING_MALLOC:
      if (MSTR_REF (s->u.string) > 1)
        s->u.string = string_unlink (s->u.string, "unlink_string_svalue");
      break;
    case STRING_SHARED:
      {
        shared_str_t shared;
        size_t len = SHARED_STRLEN (s->u.string);

        str = new_string (len, "unlink_string_svalue");
        strncpy (str, s->u.string, len + 1);
        shared = s->u.string;
        free_string (shared);
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
