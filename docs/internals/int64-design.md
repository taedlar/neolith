# Platform-Agnostic 64-bit Integer Design

## Overview

Neolith implements consistent 64-bit integer arithmetic across all platforms, eliminating the platform-specific behavior caused by `long` type differences (32-bit on Windows x64, 64-bit on Linux x64).

## Design Goals

1. **Platform Independence**: Same integer range on all platforms
2. **Backward Compatibility**: Existing LPC code continues to work
3. **Performance**: Optimize common case (32-bit values)
4. **Binary Safety**: Prevent loading incompatible saved binaries

## Implementation

### Runtime Type System

**Core Change**: `svalue_u.number` uses `int64_t` instead of `long`

```c
// lib/lpc/types.h
typedef union {
    int64_t number;      // Was: long number
    double real;
    char *string;
    // ... other fields
} svalue_u;
```

This guarantees 64-bit integers across all platforms, matching LPC semantics.

### Bytecode Representation

Two opcodes handle integer constants:

1. **F_NUMBER (opcode 8)**: Stores 32-bit integers (4 bytes)
   - Used for values in range `INT32_MIN` to `INT32_MAX`
   - Existing opcode, maintains backward compatibility
   
2. **F_LONG (opcode 10)**: Stores 64-bit integers (8 bytes)
   - Used for values outside 32-bit range
   - New opcode added for full int64_t support

**Compiler Logic** ([lib/lpc/program/icode.c](../../lib/lpc/program/icode.c)):
```c
void write_long_number(int64_t val) {
    if (val >= INT32_MIN && val <= INT32_MAX) {
        write_number((int)val);  // Use F_NUMBER
    } else {
        ins_byte(F_LONG);        // Use F_LONG
        ins_long(val);           // 8-byte encoding
    }
}
```

This optimization reduces bytecode size for common small integers while supporting the full 64-bit range.

### Byte Encoding Macros

Portable byte-level serialization handles endianness ([lib/port/byte_code.h](../../lib/port/byte_code.h)):

```c
#define LOAD_LONG(x, y)   LOAD8(x,y)   // Read 8 bytes, advance pc
#define STORE_LONG(x, y)  STORE8(x,y)  // Write 8 bytes, advance pc  
#define COPY_LONG(x, y)   COPY8(x,y)   // Copy 8 bytes, no advance
```

The `LOAD8` macro copies byte-by-byte and increments the program counter:
```c
#define LOAD8(x, y)  ((char *)&(x))[0] = *y++; \
                     ((char *)&(x))[1] = *y++; \
                     /* ... 6 more bytes ... */
```

This avoids alignment issues and ensures platform-independent byte order.

### Interpreter Implementation

**F_LONG handler** ([src/interpret.c](../../src/interpret.c)):
```c
case F_LONG:
  {
    int64_t long_val;
    LOAD_LONG(long_val, pc);  // Read 8 bytes, advance pc
    push_number(long_val);     // Push to stack
  }
  break;
```

**API Update**: `push_number()` signature changed to accept `int64_t`:
```c
void push_number(int64_t n);  // Was: void push_number(long n)
```

### Format String Handling

All integer formatting updated to use `PRId64` macro from `<inttypes.h>`:

```c
// Before (platform-specific):
sprintf(buff, "%ld", sp->u.number);

// After (portable):
sprintf(buff, "%" PRId64, sp->u.number);
```

Affected files:
- [src/interpret.c](../../src/interpret.c): String concatenation in F_ADD, F_ADD_EQ
- [src/simulate.c](../../src/simulate.c): Value-to-string conversion, debug output

Buffer sizes increased from 20 to 30 bytes to accommodate 64-bit decimal representation (max 19 digits + sign + null).

### Lexer Changes

Parser uses `strtoll()` for 64-bit literal parsing ([lib/lpc/lex.c](../../lib/lpc/lex.c)):

```c
// Decimal literals
parse_node_t *node = new_node();
node->kind = NODE_NUMBER;
node->v.number = strtoll(yytext, NULL, 10);  // Was: atoi(yytext)

// Hex/octal literals  
node->v.number = strtoll(yytext, NULL, 0);   // Was: strtol(yytext, NULL, 0)
```

Parse tree and grammar union also updated:
```c
// lib/lpc/grammar.y
%union {
    int64_t number;  // Was: long number
    // ...
}
```

### Binary Serialization Compatibility

**Driver ID Versioning** ([lib/lpc/program/binaries.c](../../lib/lpc/program/binaries.c)):

Bumped `driver_id` to invalidate old binaries:
```c
static unsigned int driver_id = 0x20260113;  // Was: 0x20251029
```

**Why This Matters**:
1. **Opcode Renumbering**: Adding F_LONG shifted all subsequent opcodes
2. **Struct Layout**: `svalue_t` size changed (8-byte alignment difference)
3. **Data Corruption**: Old binaries would interpret opcodes incorrectly

The driver checks `driver_id` during load and falls back to source compilation if mismatched.

## Critical Design Rules

### 1. Never Mix Type Domains

**Compile-time types** (`lpc_type_t`) and **runtime types** (`svalue_type_t`) are separate:

```c
// WRONG - mixing domains
if (value->type == TYPE_NUMBER) { ... }

// CORRECT - use runtime types
if (value->type == T_NUMBER) { ... }
```

See [lpc-types.md](lpc-types.md) for complete type system reference.

### 2. Always Bump driver_id When Changing Opcodes

Any change that affects bytecode layout requires `driver_id` increment:
- Adding/removing/reordering opcodes
- Changing runtime struct sizes
- Modifying instruction argument encoding

This prevents catastrophic misinterpretation of saved binaries.

### 3. Use Portable Macros for Bytecode I/O

Never read/write multi-byte values directly:

```c
// WRONG - endianness issues
int64_t val = *(int64_t*)pc;

// CORRECT - portable
int64_t val;
LOAD_LONG(val, pc);
```

## Testing Considerations

### Binary Compatibility Tests

When testing bytecode changes:
1. Delete old binaries: Remove `__SAVE_BINARIES_DIR__` contents
2. Test fresh compilation: Ensure clean compile without cached .b files
3. Verify driver_id rejection: Confirm old binaries are properly rejected

### Type Range Tests

Test boundary conditions:
- `INT32_MIN` and `INT32_MAX` (F_NUMBER boundary)
- `INT64_MIN` and `INT64_MAX` (full range)
- Mixed 32/64-bit arithmetic

## Performance Characteristics

**Memory Impact**:
- `svalue_t` size unchanged on 64-bit platforms (already 8-byte aligned)
- Windows x86 sees 4-byte increase per svalue (alignment padding)

**Bytecode Size**:
- Small integers: 5 bytes (F_NUMBER + 4-byte value) - unchanged
- Large integers: 9 bytes (F_LONG + 8-byte value) - new overhead
- Most LPC code uses small integers, so impact is minimal

**Execution Speed**:
- Native 64-bit arithmetic on all platforms
- No conversion overhead
- Stack machine cost unchanged

## Future Enhancements

Potential optimizations:
1. **F_BYTE/F_NBYTE**: Already optimize -255 to 255 range (2 bytes)
2. **Variable-length encoding**: Could compress common ranges further
3. **Constant pooling**: Share identical large constants

## Migration Notes

### Upgrading Existing Installations

1. **Rebuild Required**: Full recompilation of driver
2. **Clear Binaries**: Delete all `.b` files in `__SAVE_BINARIES_DIR__`
3. **LPC Compatibility**: No LPC source changes needed
4. **Mudlib Impact**: Transparent to LPC programmers

### API Breaking Changes

External code linking to driver must update:
- `push_number(long)` → `push_number(int64_t)`
- Any direct access to `svalue_u.number` (type changed)

## References

- **Type System**: [lpc-types.md](lpc-types.md) - Complete type encoding reference
- **Compiler Internals**: [lpc-program.md](lpc-program.md) - Memory block system
- **Opcode Reference**: [lib/efuns/func_spec.c](../../lib/efuns/func_spec.c) - Operator definitions
- **Byte Encoding**: [lib/port/byte_code.h](../../lib/port/byte_code.h) - Portable I/O macros
