#pragma once

extern size_t svalue_strlen_size;

typedef struct block_s {
    struct block_s *next;	/* next block in the hash chain */
    /* these two must be last */
    unsigned short size;	/* length of the string */
    unsigned short refs;	/* reference count    */
} block_t;

typedef struct malloc_block_s {
    block_t* unused;		/* to force MSTR_BLOCK align with block_t */
    unsigned short size;
    unsigned short ref;
} malloc_block_t;

#define MSTR_BLOCK(x) (((malloc_block_t *)(x)) - 1) 
#define MSTR_EXTRA_REF(x) (MSTR_BLOCK(x)->extra_ref)
#define MSTR_REF(x) (MSTR_BLOCK(x)->ref)
#define MSTR_SIZE(x) (MSTR_BLOCK(x)->size)
#define MSTR_UPDATE_SIZE(x, y) do {\
	ADD_STRING_SIZE(y - MSTR_SIZE(x));\
	MSTR_BLOCK(x)->size = \
	(y > USHRT_MAX ? USHRT_MAX : y);\
	} while(0)

#define FREE_MSTR(x) do {\
	DEBUG_CHECK(MSTR_REF(x) != 1, "FREE_MSTR used on a multiply referenced string\n");\
        svalue_strlen_size = MSTR_SIZE(x);\
	SUB_NEW_STRING(svalue_strlen_size, \
	sizeof(malloc_block_t));\
	FREE(MSTR_BLOCK(x));\
	SUB_STRING(svalue_strlen_size);\
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

/* This counts on some rather crucial alignment between malloc_block_t and
 * block_t.  COUNTED_STRLEN(x) is the same as strlen(sv->u.string) when
 * sv->subtype is STRING_MALLOC or STRING_SHARED, and runs significantly
 * faster.
 */
#define COUNTED_STRLEN(x) ((svalue_strlen_size = MSTR_SIZE(x)), svalue_strlen_size != USHRT_MAX ? svalue_strlen_size : strlen((x)+USHRT_MAX)+USHRT_MAX)
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

/*
 * stralloc.c
 */
extern void init_strings(void);
extern char *findstring(char *);
extern char *make_shared_string(char *);
extern char *ref_string(char *);
extern void free_string(char *);
extern void deallocate_string(char *);
extern int add_string_status(outbuffer_t *, int);
extern char *extend_string(char *, int);

#ifdef STRING_STATS
extern int num_distinct_strings;
extern int bytes_distinct_strings;
extern int allocd_strings;
extern int allocd_bytes;
extern int overhead_bytes;
#endif

extern char *int_string_copy(char *);
extern char *int_string_unlink(char *);
extern char *int_new_string(int);
extern char *int_alloc_cstring(char *);
