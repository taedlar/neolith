#pragma once

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
 *   (int_new_string, extend_string, int_string_unlink).
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

#define SVALUE_STRLEN(x) (((x)->subtype & STRING_COUNTED) ? \
                          COUNTED_STRLEN((x)->u.string) : \
                          strlen((x)->u.string))

/* For quick checks.  Avoid strlen(), etc.  This is  */
#define SVALUE_STRLEN_DIFFERS(x, y) ((((x)->subtype & STRING_COUNTED) && \
                                     ((y)->subtype & STRING_COUNTED)) ? \
                                     MSTR_SIZE((x)->u.string) != \
                                     MSTR_SIZE((y)->u.string) : 0)

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
extern char *findstring(const char *);
extern char *make_shared_string(const char *);
extern char *ref_string(char *);
extern void free_string(char *);

/* STRING_MALLOC */
extern char *int_new_string(size_t);
extern char *int_string_copy(const char *);
extern char *extend_string(char *, size_t);
extern char *int_alloc_cstring(const char *);
