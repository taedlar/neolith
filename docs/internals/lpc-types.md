# LPC Type Systems

Neolith's LPC compiler and interpreter utilize three distinct type systems, each serving a different purpose in the compilation and execution pipeline. Understanding these systems is critical for compiler development and debugging.

## Overview of Type Systems

| Type System | Typedef | Domain | Purpose | Encoding |
|------------|---------|---------|---------|----------|
| **Compile-time** | `lpc_type_t` | TYPE_* (0-10) + modifiers | Static type checking during compilation | Hybrid: sequential base + bit flags |
| **Runtime** | `svalue_type_t` (short) | T_* flags | Runtime value type dispatch | Pure bit flags (powers of 2) |
| **Parse tree** | `lpc_type_t` | TYPE_* (0-10) | Temporary type annotation in AST | Same as compile-time |

**Critical**: These systems are **incompatible**. Do not mix TYPE_* with T_* values. They serve fundamentally different purposes and use different encoding schemes.

## 1. Compile-Time Type System (`lpc_type_t`)

### Definition
```c
typedef unsigned short lpc_type_t;  /* 16-bit type value */
```
Defined in [lib/lpc/types.h](../../lib/lpc/types.h)

### Base Types (Sequential: 0-10)
```c
#define TYPE_UNKNOWN    0   /* Must be casted */
#define TYPE_ANY        1   /* Matches any type */
#define TYPE_NOVALUE    2   /* No value (e.g., void context) */
#define TYPE_VOID       3   /* void return type */
#define TYPE_NUMBER     4   /* int */
#define TYPE_STRING     5   /* string */
#define TYPE_OBJECT     6   /* object */
#define TYPE_MAPPING    7   /* mapping */
#define TYPE_FUNCTION   8   /* function pointer */
#define TYPE_REAL       9   /* float/double */
#define TYPE_BUFFER     10  /* buffer */
```
Defined in [lib/lpc/compiler.h](../../lib/lpc/compiler.h)

### Type Modifiers (Bit Flags)
```c
#define TYPE_MOD_ARRAY  0x0020  /* Array modifier (e.g., int* becomes TYPE_NUMBER | TYPE_MOD_ARRAY) */
#define TYPE_MOD_CLASS  0x0040  /* Class type modifier */
```
Defined in [lib/lpc/program.h](../../lib/lpc/program.h)

### Visibility/Storage Modifiers (NAME_TYPE_MOD flags: 0x0100-0x4000)
```c
#define NAME_HIDDEN     0x0100  /* private variables */
#define NAME_STATIC     0x0200  /* static function/variable */
#define NAME_NO_MASK    0x0400  /* nomask => not redefinable */
#define NAME_PRIVATE    0x0800  /* can't be inherited */
#define NAME_PROTECTED  0x1000  /* protected visibility */
#define NAME_PUBLIC     0x2000  /* force inherit through private */
#define NAME_VARARGS    0x4000  /* varargs function */

#define NAME_TYPE_MOD   (NAME_HIDDEN | NAME_STATIC | NAME_NO_MASK | NAME_PRIVATE | NAME_PROTECTED | NAME_PUBLIC | NAME_VARARGS)
```

### Encoding: Hybrid System
The `lpc_type_t` uses a **hybrid encoding**:
- **Bits 0-4**: Sequential base type (0-10)
- **Bit 5**: TYPE_MOD_ARRAY flag
- **Bit 6**: TYPE_MOD_CLASS flag
- **Bits 8-14**: NAME_TYPE_MOD visibility flags

**Examples**:
```c
int x;           // TYPE_NUMBER (0x0004)
int *arr;        // TYPE_NUMBER | TYPE_MOD_ARRAY (0x0024)
static int y;    // TYPE_NUMBER | NAME_STATIC (0x0204)
private int *z;  // TYPE_NUMBER | TYPE_MOD_ARRAY | NAME_PRIVATE (0x0824)
```

### Type Compatibility
Type checking uses lookup tables and bit masking:

```c
/* Compatibility matrix for base types */
lpc_type_t lpcc_compatible[11] = {
  /* UNKNOWN */ 0,
  /* ANY */     0xfff,  /* compatible with all types */
  /* NOVALUE */ CT_SIMPLE(TYPE_NOVALUE) | CT(TYPE_VOID) | CT(TYPE_NUMBER),
  /* VOID */    CT_SIMPLE(TYPE_VOID) | CT(TYPE_NUMBER),
  /* NUMBER */  CT_SIMPLE(TYPE_NUMBER) | CT(TYPE_REAL),
  /* STRING */  CT_SIMPLE(TYPE_STRING),
  /* OBJECT */  CT_SIMPLE(TYPE_OBJECT),
  /* MAPPING */ CT_SIMPLE(TYPE_MAPPING),
  /* FUNCTION */ CT_SIMPLE(TYPE_FUNCTION),
  /* REAL */    CT_SIMPLE(TYPE_REAL) | CT(TYPE_NUMBER),
  /* BUFFER */  CT_SIMPLE(TYPE_BUFFER),
};
```

**Key Functions**:
- `compatible_types(t1, t2)` - asymmetric (t1 → t2 assignment)
- `compatible_types2(t1, t2)` - symmetric (comparison operators)

Both functions mask out NAME_TYPE_MOD before checking:
```c
t1 &= ~NAME_TYPE_MOD;  // Remove visibility flags
t2 &= ~NAME_TYPE_MOD;
```

### Usage Locations

**Structures using `lpc_type_t`**:
- `compiler_function_t.type` - function return type ([lib/lpc/program.h:138](../../lib/lpc/program.h))
- `variable_t.type` - variable type ([lib/lpc/program.h:180](../../lib/lpc/program.h))
- `class_member_entry_t.type` - class member type ([lib/lpc/program.h:173](../../lib/lpc/program.h))
- `parse_node_t.type` - parse tree node type annotation ([lib/lpc/program/parse_trees.h:27](../../lib/lpc/program/parse_trees.h))

**NOTE**: `class_def_t.type` field exists but is **unused/reserved** ([lib/lpc/program.h:164](../../lib/lpc/program.h))

### Helper Macros
```c
/* Check if type is a class */
#define IS_CLASS(t) ((t & (TYPE_MOD_ARRAY | TYPE_MOD_CLASS)) == TYPE_MOD_CLASS)

/* Extract class index (remove modifiers) */
#define CLASS_IDX(t) (t & ~(NAME_TYPE_MOD | TYPE_MOD_CLASS))

/* Check type compatibility (base type only, no arrays/classes) */
#define COMP_TYPE(e, t) (!(e & (TYPE_MOD_ARRAY | TYPE_MOD_CLASS)) && \
                         (lpcc_compatible[(unsigned char)e] & (1 << (t))))

/* Check exact type match (base type only) */
#define IS_TYPE(e, t) (!(e & (TYPE_MOD_ARRAY | TYPE_MOD_CLASS)) && \
                       (lpcc_is_type[(unsigned char)e] & (1 << (t))))
```

## 2. Runtime Type System (`svalue_type_t`)

### Definition
```c
typedef short svalue_type_t;  /* 16-bit runtime type flags */

struct svalue_s {
    svalue_type_t type;  /* Runtime type tag */
    short subtype;        /* String storage subtype or T_UNDEFINED flag */
    union svalue_u u;     /* Value payload */
};
```
Defined in [lib/lpc/types.h](../../lib/lpc/types.h)

### Runtime Type Flags (Pure Bit Flags)
```c
#define T_INVALID       0x0     /* Invalid/uninitialized */
#define T_LVALUE        0x1     /* Lvalue reference */
#define T_NUMBER        0x2     /* Integer */
#define T_STRING        0x4     /* String */
#define T_ARRAY         0x8     /* Array */
#define T_OBJECT        0x10    /* Object reference */
#define T_MAPPING       0x20    /* Mapping (associative array) */
#define T_FUNCTION      0x40    /* Function pointer */
#define T_REAL          0x80    /* Float/double */
#define T_BUFFER        0x100   /* Byte buffer */
#define T_CLASS         0x200   /* Class instance */

#define T_REFED (T_ARRAY|T_OBJECT|T_MAPPING|T_FUNCTION|T_BUFFER|T_CLASS)

/* Special lvalue types */
#define T_LVALUE_BYTE   0x400   /* Byte-sized lvalue */
#define T_LVALUE_RANGE  0x800   /* Range lvalue (e.g., arr[2..5]) */
#define T_ERROR_HANDLER 0x1000  /* Error handler function */
```

### Encoding: Pure Bit Flags
Unlike `lpc_type_t`, runtime types use **pure bit flags** (powers of 2). This allows:
- Fast type checking via bitwise AND: `if (sp->type & T_NUMBER)`
- Type masking: `sp->type & T_REFED` to check reference-counted types
- No sequential encoding—each type is a unique bit

### String Subtypes
The `subtype` field provides additional type information for strings:
```c
#define STRING_COUNTED  0x1  /* Has length and ref count */
#define STRING_HASHED   0x2  /* In shared string table */
#define STRING_MALLOC   STRING_COUNTED
#define STRING_SHARED   (STRING_COUNTED | STRING_HASHED)
#define STRING_CONSTANT 0    /* Constant string (UTF-8) */

#define T_UNDEFINED     0x4  /* undefinedp() returns true */
```

### Runtime vs Compile-Time Mapping

| Compile-Time (lpc_type_t) | Runtime (svalue_type_t) |
|---------------------------|-------------------------|
| TYPE_NUMBER (4) | T_NUMBER (0x2) |
| TYPE_STRING (5) | T_STRING (0x4) |
| TYPE_OBJECT (6) | T_OBJECT (0x10) |
| TYPE_MAPPING (7) | T_MAPPING (0x20) |
| TYPE_FUNCTION (8) | T_FUNCTION (0x40) |
| TYPE_REAL (9) | T_REAL (0x80) |
| TYPE_BUFFER (10) | T_BUFFER (0x100) |
| TYPE_NUMBER \| TYPE_MOD_ARRAY | T_ARRAY (0x8) |

**WARNING**: There is **no direct numeric correspondence**. Conversion requires explicit mapping logic (not a simple cast).

### Usage
- `svalue_t.type` field in the interpreter value stack ([src/interpret.c](../../src/interpret.c))
- Runtime type dispatch in efuns ([lib/efuns/](../../lib/efuns/))
- Garbage collection and reference counting ([src/malloc.c](../../src/malloc.c))

## 3. Parse Tree Type System

### Definition
```c
typedef struct parse_node_s {
    unsigned short kind;  /* NODE_* constant (parse node type) */
    unsigned short line;  /* Source line number */
    lpc_type_t type;      /* LPC type annotation (TYPE_*) */
    /* ... union payload ... */
} parse_node_t;
```
Defined in [lib/lpc/program/parse_trees.h](../../lib/lpc/program/parse_trees.h)

### Purpose
The `parse_node_t.type` field stores compile-time type annotations during parsing and code generation. It uses the **same domain and encoding as `lpc_type_t`** (TYPE_* values 0-10 plus modifiers).

**Historical Note**: Prior to 2026-01-08, this field was `char` (8-bit), which was insufficient to represent TYPE_MOD_ARRAY (0x0020) and caused truncation. It was changed to `lpc_type_t` (16-bit) with **zero memory overhead** due to struct alignment padding.

### Memory Layout
```c
struct parse_node_s {
    unsigned short kind;  // 2 bytes (offset 0)
    unsigned short line;  // 2 bytes (offset 2)
    lpc_type_t type;      // 2 bytes (offset 4) - formerly char (1 byte) + 1 padding
    // 2 bytes padding to align union
    union { ... } // 24 bytes (offset 8)
};
// Total: 32 bytes (unchanged from when type was char)
```

### Parse Node Macros
Macros in [parse_trees.h](../../lib/lpc/program/parse_trees.h) create and manipulate parse nodes. After the type field expansion, they no longer require `(char)` casts:

```c
#define INT_CREATE_OPCODE(vn, op, t) do {\
    (vn)->v.number = op;\
    (vn)->type = t;\  /* No (char) cast needed */
    } while(0)

#define CREATE_CALL(vn, op, t, el) do {\
    (vn) = el;\
    (vn)->kind = NODE_CALL;\
    (vn)->l.number = (vn)->v.number;\
    (vn)->v.number = op;\
    (vn)->type = t;\  /* No (char) cast needed */
    } while(0)
```

## 4. Function Flags vs Type Flags

### `function_flags_t`
```c
typedef unsigned short function_flags_t;
```
Defined in [lib/lpc/program.h](../../lib/lpc/program.h)

**Purpose**: Stores function metadata **separate from type information**.

### Flag Categories

**Compilation State Flags** (0x00-0x7F):
```c
#define NAME_INHERITED      0x1   /* Function from inherited object */
#define NAME_UNDEFINED      0x2   /* Not yet defined */
#define NAME_STRICT_TYPES   0x4   /* Compiled with strict types */
#define NAME_PROTOTYPE      0x8   /* Only prototype declared */
#define NAME_DEF_BY_INHERIT 0x10  /* Defined in inherited object */
#define NAME_ALIAS          0x20  /* Alias to another function */
#define NAME_TRUE_VARARGS   0x40  /* True varargs function */
```

**Visibility/Storage Flags** (0x0100-0x4000) - **Shared with lpc_type_t**:
```c
#define NAME_HIDDEN     0x0100  /* private variables */
#define NAME_STATIC     0x0200  /* static */
#define NAME_NO_MASK    0x0400  /* nomask */
#define NAME_PRIVATE    0x0800  /* private */
#define NAME_PROTECTED  0x1000  /* protected */
#define NAME_PUBLIC     0x2000  /* public */
#define NAME_VARARGS    0x4000  /* varargs */

#define NAME_TYPE_MOD   (NAME_HIDDEN | NAME_STATIC | NAME_NO_MASK | NAME_PRIVATE | NAME_PROTECTED | NAME_PUBLIC | NAME_VARARGS)
```

### Separation of Concerns
When defining a function, the compiler splits type information:
```c
compiler_function_t func;
func.type = return_type;  // lpc_type_t (e.g., TYPE_NUMBER | NAME_STATIC)

function_flags_t flags = NAME_STRICT_TYPES | (return_type & NAME_TYPE_MOD);
// Extract visibility flags from type, combine with compilation flags
```

See `define_new_function()` in [lib/lpc/compiler.c](../../lib/lpc/compiler.c) for the splitting logic.

### Key Macros
```c
#define NAME_MASK (NAME_UNDEFINED | NAME_STRICT_TYPES | NAME_PROTOTYPE | NAME_TRUE_VARARGS | NAME_TYPE_MOD)
/* Flags copied through inheritance */

#define NAME_NO_CODE (NAME_UNDEFINED | NAME_ALIAS | NAME_PROTOTYPE)
/* Function isn't 'real' (no executable code) */

#define REAL_FUNCTION(x) (!((x) & (NAME_ALIAS | NAME_PROTOTYPE)) && \
                         (((x) & NAME_DEF_BY_INHERIT) || (!((x) & NAME_UNDEFINED))))
/* Function has executable code */
```

## 5. Common Pitfalls and Best Practices

### Don't Mix Type Systems
```c
/* WRONG - mixing compile-time and runtime types */
if (svalue->type == TYPE_NUMBER) { ... }  // TYPE_NUMBER = 4, T_NUMBER = 0x2

/* CORRECT */
if (svalue->type & T_NUMBER) { ... }
```

### Masking Visibility Flags
When checking types, always mask NAME_TYPE_MOD:
```c
lpc_type_t base_type = func->type & ~NAME_TYPE_MOD;
if (base_type == TYPE_NUMBER) { ... }
```

### Array Type Detection
```c
/* Check if type is an array */
if (type & TYPE_MOD_ARRAY) { ... }

/* Get element type of array */
lpc_type_t elem_type = type & ~(TYPE_MOD_ARRAY | NAME_TYPE_MOD);
```

### Class Type Handling
```c
/* Check if type is a class (not an array) */
if (IS_CLASS(type)) {
    int class_index = CLASS_IDX(type);  // Extract class definition index
    class_def_t *cdef = CLASS(class_index);
}
```

### Type Compatibility Checking
```c
/* For assignment: can t1 be assigned to t2? */
if (compatible_types(t1, t2)) { ... }

/* For comparison: are t1 and t2 compatible for ==, !=, etc? */
if (compatible_types2(t1, t2)) { ... }
```

### Parse Tree Type Consistency
When creating parse nodes, ensure type values are in the TYPE_* domain:
```c
CREATE_OPCODE(node, F_ADD, TYPE_NUMBER);  // CORRECT
CREATE_OPCODE(node, F_ADD, T_NUMBER);     // WRONG - runtime type, not compile-time
```

## 6. Type System Evolution

### Recent Changes
- **2026-01-08**: Converted `parse_node_t.type` from `char` to `lpc_type_t` for type safety and to support TYPE_MOD_ARRAY flag representation. This change had zero memory cost due to alignment padding.

### Future Considerations
1. **Type unification**: The `unsigned short` fields in program.h structures (compiler_function_t.type, variable_t.type, class_member_entry_t.type) should be consistently typedef'd as `lpc_type_t` for clarity.

2. **Class type metadata**: The `class_def_t.type` field is currently unused/reserved. Consider documenting its intended purpose or removing it.

3. **Extended type system**: Bits 15 in `lpc_type_t` is currently unused, providing room for future type modifiers if needed.

## 7. Reference Tables

### Type System Quick Reference

| Context | Typedef | Size | Domain | Check Method |
|---------|---------|------|--------|--------------|
| Compilation | `lpc_type_t` | 16-bit | TYPE_* (0-10) + flags | `compatible_types()` |
| Parse tree | `lpc_type_t` | 16-bit | TYPE_* (0-10) + flags | Same as compilation |
| Runtime | `svalue_type_t` | 16-bit | T_* bit flags | Bitwise AND |
| Function metadata | `function_flags_t` | 16-bit | NAME_* flags | Bitwise operations |

### Bit Layout Comparison

**lpc_type_t (compile-time)**:
```
Bits:  15-14-13-12-11-10-09-08 | 07-06-05-04-03-02-01-00
       [ NAME_TYPE_MOD flags  ] | U |CLASS|ARRAY|  BASE  |
                                   n                (0-10)
                                   u
                                   s
                                   e
                                   d
```

**svalue_type_t (runtime)**:
```
Bits:  15-14-13-12-11-10-09-08 | 07-06-05-04-03-02-01-00
       [  unused  ]ERR|RNG|BYTE| CLS|BUF|REAL|FN|MAP|OB|ARR|STR|NUM|LV|INV
```

**function_flags_t**:
```
Bits:  15-14-13-12-11-10-09-08 | 07-06-05-04-03-02-01-00
       [ NAME_TYPE_MOD flags  ] | [  Compilation state  ]
```

## Related Documentation
- [LPC Program Structure](lpc-program.md) - Memory blocks and program layout
- [LPC Compiler Architecture](../manual/internals.md#lpc-compiler-architecture) - Compilation pipeline
- [Function Dispatch](lpc-program.md#function-dispatch) - Runtime function lookup
