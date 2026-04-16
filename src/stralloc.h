#pragma once

/* for USHRT_MAX */
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Typed aliases for runtime string storage.
 *
 * shared_str_t: a char * known to be a STRING_SHARED payload (block_t header
 *               immediately precedes the pointer; managed by the shared string
 *               table).
 *
 * malloc_str_t: a char * known to be a STRING_MALLOC payload (malloc_block_t
 *               header immediately precedes the pointer).
 *
 * Contract-bearing APIs use explicit handle types (shared_str_handle_t /
 * malloc_str_handle_t). Under STRING_TYPE_SAFETY these are abstract handles
 * that require explicit bridge conversion. Otherwise they collapse to raw
 * pointers for compatibility.
 */
typedef char *shared_str_t;  /* STRING_SHARED payload pointer storage alias */
typedef char *malloc_str_t;  /* STRING_MALLOC payload pointer storage alias */

#ifdef STRING_TYPE_SAFETY
typedef struct {
        char *raw;
} shared_str_handle_t;

typedef struct {
        char *raw;
} malloc_str_handle_t;

#define SHARED_STR_P(x) ((x).raw)
#define MALLOC_STR_P(x) ((x).raw)

static inline shared_str_handle_t to_shared_str(shared_str_t p) {
        shared_str_handle_t h = { p };
        return h;
}

static inline malloc_str_handle_t to_malloc_str(malloc_str_t p) {
        malloc_str_handle_t h = { p };
        return h;
}
#else
typedef char *shared_str_handle_t;
typedef char *malloc_str_handle_t;

#define SHARED_STR_P(x) (x)
#define MALLOC_STR_P(x) (x)
#define to_shared_str(x) (x)
#define to_malloc_str(x) (x)
#endif

struct outbuffer_s;
typedef struct outbuffer_s outbuffer_t;
/**
 * Shared string (STRING_SHARED) block header.
 * The string hash table consists of linked lists of these blocks.
 * - A shared string is reference counted and should be used as right-hand
 *   side of an assignment only.
 * - A shared string is always freed through release of its reference count.
 */
typedef struct block_s {
    struct block_s *next;	/* next block in the hash chain */
    /* these two must be last */
    unsigned short size;	/* length of the string */
    unsigned short refs;	/* reference count    */
} block_t;

/**
 * Malloc block header for STRING_MALLOC strings.
 * The layout is designed to align with block_t for efficient access.
 *
 * - MSTR_* macros depend on this structure.
 * - MSTR_REF and MSTR_SIZE are compatible with both STRING_MALLOC and STRING_SHARED.
 * - A malloc string is NOT added to the shared string hash table. Its reference count
 *   is set to 1 on creation so it can be freed via free_string_svalue() without sharing.
 *
 * Length representation:
 * - When size < USHRT_MAX: size holds the exact string length.
 * - When size == USHRT_MAX (sentinel): the string is longer than USHRT_MAX - 1 bytes.
 *   blkend points one past the last byte of the string payload, allowing O(1) length
 *   recovery via (char*)blkend - str.  blkend is set by all allocation/resize paths
 *   (new_string, extend_string, string_unlink).
 *
 * Invariant: STRING_SHARED strings are always shorter than USHRT_MAX bytes, so the
 * sentinel case is exclusive to STRING_MALLOC. Generic counted-string macros that
 * read size == USHRT_MAX may therefore safely use blkend without subtype dispatch.
 */
typedef struct malloc_block_s {
    void *blkend;   /* end of string payload; non-null only when size == USHRT_MAX */
    unsigned short size;
    unsigned short ref;
} malloc_block_t;

#define MSTR_BLOCK(x) (((malloc_block_t *)(x)) - 1)
#define MSTR_REF(x) (MSTR_BLOCK(x)->ref)
#define MSTR_SIZE(x) (MSTR_BLOCK(x)->size)
#define MSTR_BLKEND(x) (MSTR_BLOCK(x)->blkend)
#define MSTR_UPDATE_SIZE(x, y) do {\
        ADD_STRING_SIZE(y - MSTR_SIZE(x));\
        MSTR_BLOCK(x)->size = \
        (y > USHRT_MAX ? USHRT_MAX : y);\
        MSTR_BLOCK(x)->blkend = (y >= USHRT_MAX ? (void *)((x) + (y)) : (void *)0);\
        } while(0)

#define FREE_MSTR(x) do {\
        unsigned short size_mstr = MSTR_SIZE(x);\
        DEBUG_CHECK(MSTR_REF(x) != 1, "FREE_MSTR used on a multiply referenced string\n");\
        SUB_NEW_STRING(size_mstr, sizeof(malloc_block_t));\
        FREE(MSTR_BLOCK(x));\
        SUB_STRING(size_mstr);\
        } while(0)

#ifdef STRING_STATS
#define ADD_NEW_STRING(len, overhead) num_distinct_strings++; bytes_distinct_strings += len + 1; overhead_bytes += overhead
#define SUB_NEW_STRING(len, overhead) num_distinct_strings--; bytes_distinct_strings -= len + 1; overhead_bytes -= overhead

#define ADD_STRING(len) allocd_strings++; allocd_bytes += len + 1
#define ADD_STRING_SIZE(len) allocd_bytes += len; bytes_distinct_strings += len
#define SUB_STRING(len) allocd_strings--; allocd_bytes -= len + 1
#else
/* Blazing fast macros :) */
#define ADD_NEW_STRING(x, y)
#define SUB_NEW_STRING(x, y)
#define ADD_STRING(x)
#define ADD_STRING_SIZE(x)
#define SUB_STRING(x)
#endif

/*
 * COUNTED_STRLEN(x) returns the logical length of a counted string (STRING_MALLOC
 * or STRING_SHARED) without scanning for a NUL byte for short strings.
 *
 * Three cases:
 *   size < USHRT_MAX  => exact length, returned directly (O(1)).
 *   size == USHRT_MAX and blkend != NULL => long STRING_MALLOC; pointer-difference
 *                            gives length in O(1) without NUL scanning.
 *   size == USHRT_MAX and blkend == NULL => legacy/compatibility path; falls back
 *                            to strlen starting at offset USHRT_MAX.
 *
 * The two-pointer alignment between malloc_block_t and block_t is still required
 * so that MSTR_SIZE and MSTR_REF work uniformly on both string kinds.
 */
#define COUNTED_STRLEN(x) ((MSTR_SIZE(x) == USHRT_MAX) ? \
        (MSTR_BLKEND(x) ? (size_t)((char *)MSTR_BLKEND(x) - (x)) : (strlen((x)+USHRT_MAX)+USHRT_MAX)) : \
        MSTR_SIZE(x))

/* return the number of references to a STRING_MALLOC or STRING_SHARED 
   string */
#define COUNTED_REF(x)    MSTR_REF(x)

/* ref == 0 means the string has been referenced USHRT_MAX times and is
   immortal */
#define INC_COUNTED_REF(x) if (MSTR_REF(x)) MSTR_REF(x)++;
/* This is a conditional expression that evaluates to zero if the block
   should be deallocated */
#define DEC_COUNTED_REF(x) (!(MSTR_REF(x) == 0 || --MSTR_REF(x) > 0))

#define SHARED_STRLEN(x) COUNTED_STRLEN(x)

#define SVALUE_COUNTED_STRLEN(x) (((x)->subtype == STRING_MALLOC) ? \
                                  COUNTED_STRLEN((x)->u.malloc_string) : \
                                  COUNTED_STRLEN((x)->u.shared_string))

#define SVALUE_STRPTR(x) ((char *)(((x)->subtype == STRING_MALLOC) ? \
                          (x)->u.malloc_string : \
                          (((x)->subtype == STRING_SHARED) ? \
                           (x)->u.shared_string : \
                           (x)->u.const_string)))

#define SVALUE_STRLEN(x) (((x)->subtype & STRING_COUNTED) ? \
                          SVALUE_COUNTED_STRLEN(x) : \
                          strlen((x)->u.const_string))

/*
 * Compare two svalue string lengths.
 * - Counted/counting pairs use counted logical lengths (handles blkend long strings).
 * - Any path involving STRING_CONSTANT falls back to SVALUE_STRLEN(), which uses strlen.
 */
#define SVALUE_STRLEN_DIFFERS(x, y) ((((x)->subtype & STRING_COUNTED) && ((y)->subtype & STRING_COUNTED)) \
        ? (SVALUE_COUNTED_STRLEN(x) != SVALUE_COUNTED_STRLEN(y)) \
        : (SVALUE_STRLEN(x) != SVALUE_STRLEN(y)))

extern void init_strings(size_t hash_size, size_t max_len);
extern void deinit_strings();

#ifdef STRING_STATS
extern int num_distinct_strings;
extern size_t bytes_distinct_strings;
extern int allocd_strings;
extern size_t allocd_bytes;
extern size_t overhead_bytes;
#endif
extern size_t add_string_status(outbuffer_t *, int);

/* STRING_SHARED */
extern shared_str_t findstring(const char *, const char *);
extern shared_str_t make_shared_string(const char *, const char *);
extern shared_str_t ref_string(shared_str_handle_t);
extern void free_string(shared_str_handle_t);

/* STRING_MALLOC */
extern malloc_str_t int_new_string(size_t);
extern malloc_str_t int_string_copy(const char *, const char *);
extern malloc_str_t int_extend_string(malloc_str_handle_t, size_t);
extern malloc_str_t int_string_unlink (malloc_str_handle_t);
extern char *int_alloc_cstring(const char *, const char *);

#ifdef STRING_TYPE_SAFETY
static inline int is_shared_string_payload(shared_str_t p) {
        unsigned short size;

        if (p == 0)
                return 0;

        /* Shared strings are always shorter than USHRT_MAX by invariant. */
        size = MSTR_SIZE(p);
        if (size == USHRT_MAX)
                return 0;

#ifdef STRING_TYPE_SAFETY_STRICT
        {
                size_t len = SHARED_STRLEN(p);
                return findstring(p, len ? p + len : NULL) == p;
        }
#else
        return 1;
#endif

}

static inline int is_malloc_string_payload(malloc_str_t p) {
        unsigned short size;

        if (p == 0)
                return 0;

        size = MSTR_SIZE(p);

        /*
         * Fast-path classifier for hot boundaries (push/put malloced string):
         * - size == USHRT_MAX: this is a long STRING_MALLOC sentinel form.
         * - size < USHRT_MAX: ambiguous with STRING_SHARED; do not inspect
         *   MSTR_BLKEND here because pointer provenance is unknown and that
         *   field is not layout-compatible with block_t for classification.
         */
        if (size == USHRT_MAX)
                return 1;

#ifdef STRING_TYPE_SAFETY_STRICT
        {
                size_t len = COUNTED_STRLEN(p);
                return findstring(p, len ? p + len : NULL) != p;
        }
#else
        return 1;
#endif
}
#endif

#ifdef __cplusplus
}
#endif
