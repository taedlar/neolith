#pragma once
#include "types.h"

/*
 * A compiled program consists of several data blocks, all allocated
 * contiguously in memory to enhance the working set. During compilation,
 * the blocks will be allocated separately, as the final size is
 * unknown. When compilation is done, the blocks will be copied into
 * the one big area.
 *
 * There are 5 different blocks of information for each program:
 * 1. The program itself. Consists of machine code instructions for a virtual
 *    stack machine. The size of the program must not be bigger than
 *    65535 bytes, as 16 bit pointers are used. Who would ever need a bigger
 *    program :-)
 * 2. Function names. All local functions that has been defined or called,
 *    with the address of the function in the program. Inherited functions
 *    will be found here too, with information of how far up the inherit
 *    chain that the function was defined.
 * 3. String table. All strings used in the program. They are all pointers
 *    into the shared string area. Thus, they are easily found and deallocated
 *    when the object is destructed.
 * 4. Table of variable names. They all point into the shared string table.
 * 5. Line number information. A table which tells at what address every
 *    line belongs to. The table has the same number of entries as the
 *    programs has source lines. This is used at errors, to find out the
 *    line number of the error.  This is usually swapped out to save space.
 *    First entry is the length of the table.
 * 6. List of inherited objects.
 */

typedef unsigned short function_flags_t;    /* function flags saved in A_FUNCTION_FLAGS block */

/*
 * When a new object inherits from another, all function definitions
 * are copied, and all variable definitions.
 * Flags below can't explicitly declared. Flags that can be declared,
 * are found with TYPE_ below.
 *
 * When an object is compiled with type testing NAME_STRICT_TYPES, all
 * types are saved of the arguments for that function during compilation.
 * If the #pragma save_types is specified, then the types are saved even
 * after compilation, to be used when the object is inherited.
 */

/* NAME_INHERITED - The function entry that exists in this object actually
                    is a function in an object we inherited
 * NAME_UNDEFINED - the function hasn't been defined yet at this level
 * NAME_STRICT_TYPES - compiled with strict type testing
 * NAME_PROTOTYPE - only a prototype has been found so far
 * NAME_DEF_BY_INHERIT - this function actually exists in an object we've
                         inherited; if we don't find a function at this level
                         we'll use that one
 * NAME_ALIAS     - This entry refers us to another entry, usually because
                    this function was overloaded by that function
 */

#define NAME_INHERITED      0x1
#define NAME_UNDEFINED      0x2
#define NAME_STRICT_TYPES   0x4
#define NAME_PROTOTYPE      0x8
#define NAME_DEF_BY_INHERIT 0x10
#define NAME_ALIAS          0x20
#define NAME_TRUE_VARARGS   0x40

#define NAME_HIDDEN	        0x0100  /* used by private vars */
#define NAME_STATIC	        0x0200	/* Static function or variable */
#define NAME_NO_MASK        0x0400	/* The nomask => not redefineable */
#define NAME_PRIVATE        0x0800	/* Can't be inherited */
#define NAME_PROTECTED      0x1000
#define NAME_PUBLIC         0x2000	/* Force inherit through private */
#define NAME_VARARGS        0x4000	/* Used for type checking */

#define NAME_TYPE_MOD		(NAME_HIDDEN | NAME_STATIC | NAME_NO_MASK | NAME_PRIVATE | NAME_PROTECTED | NAME_PUBLIC | NAME_VARARGS)
/* only the flags that should be copied up through inheritance levels */
#define NAME_MASK (NAME_UNDEFINED | NAME_STRICT_TYPES | NAME_PROTOTYPE | NAME_TRUE_VARARGS | NAME_TYPE_MOD)
/* a function that isn't 'real' */
#define NAME_NO_CODE  (NAME_UNDEFINED | NAME_ALIAS | NAME_PROTOTYPE)
#define REAL_FUNCTION(x) (!((x) & (NAME_ALIAS | NAME_PROTOTYPE)) && \
                         (((x) & NAME_DEF_BY_INHERIT) || (!((x) & NAME_UNDEFINED))))
/*
 * These are or'ed in on top of the basic type.
 */
#define TYPE_MOD_ARRAY      0x0020	/* Pointer to a basic type */
#define TYPE_MOD_CLASS      0x0040  /* a class */

typedef unsigned short function_index_t; /* an integer type for program_t's function_offsets indices (runtime_function_u) */
typedef unsigned short function_number_t; /* an integer type for program_t's function_table indices (compiler_function_t) */
typedef unsigned short function_address_t; /* an integer type for function addresses in the program (char) */

/***** Area A_RUNTIME_FUNCTIONS *****/
typedef struct runtime_defined_s
{
    unsigned char num_arg;
    unsigned char num_local;
    function_number_t f_index; /* index in the A_COMPILER_FUNCTIONS area */
}
runtime_defined_t;

typedef struct runtime_inherited_s
{
    unsigned short offset;
    function_index_t function_index_offset;
}
runtime_inherited_t;

typedef union
{
    runtime_defined_t def;
    runtime_inherited_t inh;
}
runtime_function_u; /* runtime function table entry in A_RUNTIME_FUNCTIONS area */

/***** Area A_RUNTIME_COMPRESSED *****/
typedef struct compressed_offset_table_s
{
    function_index_t first_defined;
    function_index_t first_overload; 
    unsigned short num_compressed;
    unsigned short num_deleted;
    unsigned char index[1];
}
compressed_offset_table_t;

/***** Area A_COMPILER_FUNCTIONS *****/
struct compiler_function_s
{
    char *name;
    unsigned short type;
    function_index_t runtime_index; /* index into A_FUNCTION_FLAGS area */
    function_address_t address;
}; /* function definition entry in A_COMPILER_FUNCTIONS area */

/***** Area A_FUNCTION_DEFS *****/
typedef struct compiler_temp_s
{
    struct program_s *prog; /* inherited if nonzero */
    union {
        compiler_function_t *func;
        function_index_t index;
    } u;
    /* For non-aliases, this is a count of the number of non-aliases we've
       seen for this function. */
    unsigned short alias_for;
}
compiler_temp_t;

/***** Area A_CLASS_DEFS *****/
typedef struct class_def_s
{
    unsigned short name;
    unsigned short type;
    unsigned short size;
    unsigned short index;
}
class_def_t;

/***** Area A_CLASS_MEMBER *****/
typedef struct class_member_entry_s
{
    unsigned short name;
    unsigned short type;
}
class_member_entry_t;

/***** Area A_VAR_NAME and A_VAR_TEMP *****/
typedef struct variable_s
{
    char *name;
    unsigned short type;	/* Type of variable. See above. TYPE_ */
}
variable_t;

/***** Area A_INHERIT *****/
typedef struct inherit_s
{
    struct program_s *prog;
    unsigned short function_index_offset;
    unsigned short variable_index_offset;
    unsigned short type_mod;
}
inherit_t;

/***** The program structure *****/
struct program_s
{
    char *name;	                /* Name of file that defined prog */
    int flags;
    unsigned short ref;	        /* Reference count */
    unsigned short func_ref;
    char *program;              /* The binary instructions (A_PROGRAM area) */
    int id_number;              /* used to associate information with this
                                 * prog block without needing to increase the
                                 * reference count     */
    unsigned char *line_info;   /* Line number information (A_LINENUMBERS area) */
    unsigned short *file_info;  /* File information (A_FILE_INFO area)*/
    compiler_function_t *function_table;
    function_flags_t *function_flags; /* separate for alignment reasons */
    runtime_function_u *function_offsets;
#ifdef COMPRESS_FUNCTION_TABLES
    compressed_offset_table_t *function_compressed;
#endif
    class_def_t *classes;
    class_member_entry_t *class_members;
    char **strings;	        /* All strings uses by the program */
    char **variable_table;  /* variables defined by this program */
    unsigned short *variable_types;	/* variables defined by this program */
    inherit_t *inherit;     /* List of inherited prgms */
    int total_size;	        /* Sum of all data in this struct */
    int heart_beat;	        /* Index of the heart beat function. -1 means no heart beat */
    /*
     * The types of function arguments are saved where 'argument_types'
     * points. It can be a variable number of arguments, so allocation is
     * done dynamically. To know where first argument is found for function
     * 'n' (number of function), use 'type_start[n]'. These two arrays will
     * only be allocated if '#pragma save_types' has been specified. This
     * #pragma should be specified in files that are commonly used for
     * inheritance. There are several lines of code that depends on the type
     * length (16 bits) of 'type_start' (sorry !).
     */
    unsigned short *argument_types;
#define INDEX_START_NONE    65535
    unsigned short *type_start;
    /*
     * And now some general size information.
     */
    unsigned short program_size;    /* size of this instruction code */
    unsigned short num_classes;
    function_number_t num_functions_total;
    function_number_t num_functions_defined;
    unsigned short num_strings;
    unsigned short num_variables_total;
    unsigned short num_variables_defined;
    unsigned short num_inherited;
};

extern int total_num_prog_blocks;
extern int total_prog_block_size;
void reference_prog(program_t *, char *);
void free_prog(program_t *, int);
void deallocate_program(program_t *);
char *variable_name(program_t *, int);
char *function_name(program_t *, int);
runtime_function_u *find_func_entry(const program_t *, int);

/* the simple version */
#define FUNC_ENTRY(p, i) ((p)->function_offsets + (i))
#ifdef COMPRESS_FUNCTION_TABLES
/* Find a function entry */
#define FIND_FUNC_ENTRY(p, i) (((i) < (p)->function_compressed->first_defined) ? find_func_entry(p, i) : FUNC_ENTRY(p, (i) - (p)->function_compressed->num_deleted))
#else
#define FIND_FUNC_ENTRY(p, i) FUNC_ENTRY(p, i)
#endif

int translate_absolute_line(int, unsigned short *, size_t, int *, int *);
