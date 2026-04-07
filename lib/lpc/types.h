#pragma once

#ifdef HAVE_STDINT_H
    #include <stdint.h>
#elif HAVE_INTTYPES_H
    #include <inttypes.h>
#endif

/*
 * Typed aliases for contract-bearing char * parameters.
 *
 * shared_str_t: a char * known to be a STRING_SHARED payload (block_t header
 *               immediately precedes the pointer; managed by the shared string
 *               table).  Pass to ref_string() and free_string().
 *
 * malloc_str_t: a char * known to be a STRING_MALLOC payload (malloc_block_t
 *               header immediately precedes the pointer).  Pass to
 *               extend_string(); returned by new_string() and
 *               string_copy().
 *
 * In all build modes the typedefs are transparent (identical to char *), so no
 * existing call sites require changes and there is no runtime overhead.
 * When STRING_TYPE_SAFETY is defined (default ON), the boundary functions
 * additionally validate their pointer contract at runtime even in release builds.
 *
 * Path to full compile-time enforcement: change the typedefs to opaque struct
 * pointer types, update struct fields / variables that store typed strings, and
 * add SHARED_STR()/MALLOC_STR() cast macros at call sites where conversion is
 * needed.
 */
typedef char *shared_str_t;  /* STRING_SHARED payload pointer */
typedef char *malloc_str_t;  /* STRING_MALLOC payload pointer */

typedef unsigned short lpc_type_t; /* entry type in A_ARGUMENT_TYPES area */
typedef unsigned short function_index_t; /* an integer type for LPC function index (runtime_function_u) */

/* forward declarations */
typedef struct array_s			array_t;
typedef struct buffer_s			buffer_t;
typedef struct compiler_function_s	compiler_function_t;
typedef struct funptr_s			funptr_t;
typedef struct mapping_s		mapping_t;
typedef struct object_s			object_t;
typedef struct program_s		program_t;
typedef struct sentence_s		sentence_t;
typedef struct svalue_s			svalue_t;
typedef struct userid_s			userid_t;

typedef struct {
    unsigned short ref;
} refed_t;

union svalue_u {
    /* C-string semantics*/
    char *string;
    const char *const_string;
    shared_str_t shared_string;
    malloc_str_t malloc_string;

    int64_t number;    /* Neolith extension: fixed 64-bit integer for consistent cross-platform semantics */
    double real;    /* Neolith extension: both float and double are in native double precision */

    refed_t *refed; /* any of the block below */

    struct buffer_s *buf;
    struct object_s *ob;
    struct array_s *arr;
    struct mapping_s *map;
    struct funptr_s *fp;

    struct svalue_s *lvalue;
    unsigned char *lvalue_byte;
    void (*error_handler) (void);
};

/** @brief The value stack element.
 *  If it is a string, then the way that the string has been allocated differently, which will affect how it should be freed.
 */
typedef short svalue_type_t;
struct svalue_s {
    svalue_type_t type; /* runtime type of svalue_t (bit flags, not to be confused with lpc_type_t) */
    short subtype;
    union svalue_u u;
};



/* values for type field of svalue struct */
#define T_INVALID       0x0
#define T_LVALUE        0x1

#define T_NUMBER        0x2
#define T_STRING        0x4
#define T_REAL          0x80

#define T_ARRAY         0x8
#define T_OBJECT        0x10
#define T_MAPPING       0x20
#define T_FUNCTION      0x40
#define T_BUFFER        0x100
#define T_CLASS         0x200

#define T_REFED (T_ARRAY|T_OBJECT|T_MAPPING|T_FUNCTION|T_BUFFER|T_CLASS)

#define T_LVALUE_BYTE   0x400   /* byte-sized lvalue */
#define T_LVALUE_RANGE  0x800
#define T_ERROR_HANDLER 0x1000
#define T_ANY T_STRING|T_NUMBER|T_ARRAY|T_OBJECT|T_MAPPING|T_FUNCTION| \
        T_REAL|T_BUFFER|T_CLASS

/* values for subtype field of svalue struct */
#define STRING_COUNTED  0x1     /* has a length an ref count */
#define STRING_HASHED   0x2     /* is in the shared string table */

#define STRING_MALLOC   STRING_COUNTED
#define STRING_SHARED   (STRING_COUNTED | STRING_HASHED)
#define STRING_CONSTANT 0       /* constant string, always in multi-byte encoding (UTF-8) */

#define T_UNDEFINED     0x4     /* undefinedp() returns true */
