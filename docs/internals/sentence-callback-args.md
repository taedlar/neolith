# Sentence Callback Arguments Design

**Status**: Implemented (2026-02-10)  
**Branch**: sentence_enhance  

---

## Overview

Unified callback argument handling by storing carryover arguments in `sentence_t->args` instead of `interactive_t->carryover`. This eliminates architectural redundancy, enables `add_action()` varargs, and simplifies the codebase.

**Key Insight**: Both `input_to()` and `add_action()` use `sentence_t` for callbacks. Adding a single `args` field enables both efuns to store carryover arguments in the same location while maintaining correct LPC argument order.

---

## Architecture

### Data Structures

#### sentence_t (lib/lpc/object.h)

```c
struct sentence_s {
    char *verb;                 /* command verb (for add_action) */
    struct sentence_s *next;
    object_t *ob;               /* object owning the function */
    string_or_func_t function;  /* union: either string name or funptr_t* */
    int flags;                  /* V_FUNCTION bit indicates funptr vs string */
    array_t *args;              /* ⭐ NEW: carryover arguments */
};
```

**Ownership**: `sentence_t` owns its `args` array (freed in `free_sentence()`).

#### interactive_t (src/comm.h)

**REMOVED FIELDS**:
- ~~`svalue_t *carryover`~~ ❌
- ~~`int num_carry`~~ ❌

These fields were redundant - callback state belongs with the callback (in `sentence_t`), not in the interactive connection state.

### Callback Patterns

Both `input_to()` and `add_action()` follow a unified pattern:

1. Convert string callbacks to function pointers (or use existing funptr)
2. Store function pointer in `sentence->function.f`, carryover args in `sentence->args`
3. At invocation: push primary arg first, then carryover args from `sentence->args`
4. Cleanup: `free_sentence()` frees both function pointer and args array

See [src/simulate.c](../../src/simulate.c) for `input_to()` and `add_action()` implementations.

### LPC Argument Order

**Critical Requirement**: Carryover args must come AFTER primary args per LPC spec.

**input_to() example**:
```c
input_to("callback", 0, arg1, arg2);
// User input: "hello"
// Calls: callback("hello", arg1, arg2)  // Input FIRST, args AFTER
```

**add_action() example**:
```c
add_action("do_climb", "climb", 0, player, context);
// User input: "climb wall"  
// Calls: do_climb("wall", player, context)  // Command args FIRST, carryover AFTER
```

**Implementation** (see [src/comm.c:call_function_interactive()](../../src/comm.c)):
- Push primary arg first
- Push carryover args from `sentence->args` after
- Cannot use `funptr->hdr.args` (merge order is reversed)

### Function Pointer Conversion

String callbacks are converted to `FP_LOCAL` function pointers at setup time:

```c
if (fun->type == T_STRING) {
    callback_funp = make_lfun_funp_by_name(fun->u.string, &dummy);
} else if (fun->type == T_FUNCTION) {
    callback_funp = fun->u.fp;
    callback_funp->hdr.ref++;
}
```

**Rationale**: Fail-fast error handling (function not found reported immediately), unified code path (always V_FUNCTION at invocation), no redundant lookups.

### Helper Function: make_lfun_funp_by_name()

Creates `FP_LOCAL` function pointer from function name. Implementation in [lib/lpc/functional.c](../../lib/lpc/functional.c).

**Signature**: `funptr_t* make_lfun_funp_by_name(const char *name, svalue_t *args);`

**Note**: We don't use `funptr->hdr.args` for carryover arguments because `merge_arg_lists()` puts bound args BEFORE pushed args (wrong order for our use case).

---

## Backward Compatibility

100% compatible - all existing code works unchanged. `add_action()` varargs is an extension that doesn't break existing usage.

---

## Memory Management

**Ownership**: `sentence_t` owns `args` array from creation until destruction.

**Allocation**: `allocate_empty_array()` creates array with ref=1, owned by sentence.

**Cleanup**: `free_sentence()` frees both function pointer and args array.

**Transfer**: `call_function_interactive()` temporarily increments ref, transfers to stack, then frees.

See [lib/lpc/object.c:free_sentence()](../../lib/lpc/object.c) for implementation.

---

## Testing

**Test File**: [test_input_to_get_char.cpp](../../tests/test_lpc_interpreter/test_input_to_get_char.cpp)

**Coverage** (17 tests, all passing):
- String and function pointer callbacks
- Carryover argument handling  
- Single-char mode (`get_char`)
- Nested `input_to` calls
- Flags (I_NOECHO, I_NOESC)
- LPC spec compliance (argument order)
- Error handling

---

## add_action() Extension

**New Signature**: `varargs void add_action(string | function, string | string *, void | int, ...);`

**Use Case**: Pass context from `init()` to command handler:

```c
void init() {
    add_action("cmd_attack", "attack", 0, this_player(), (["zone": "safe"]));
}

int cmd_attack(string args, object who, mapping zone) {
    // args = "orc", who = player, zone = context
}
```

**Implementation**: Same pattern as `input_to()` - carryover args in `sentence->args`, command args pushed first.

---

## Design Rationale

**Why store args in sentence_t?** Unifies callback state - function and arguments in one structure for cleaner ownership and simpler cleanup.

**Why convert strings to function pointers?** Fail-fast error handling, unified code path, no redundant lookups on every user input.

**Why not use funptr->hdr.args?** Wrong argument order - `merge_arg_lists()` puts bound args BEFORE pushed args, but we need primary arg FIRST, carryover AFTER.

---

## Performance Impact

**Memory**: +~20 bytes per callback (function pointer overhead), -16 bytes in `interactive_t`

**Runtime**: No measurable impact (one-time O(log F) function lookup, called seconds apart)

---

## Integration Notes

Prerequisite for comm-separation refactor - eliminates need for `comm_get/set_carryover_args()` accessor functions since callback state remains in `sentence_t`.

---

## References

- **Implementation Plan**: [docs/plan/sentence-args-refactor-plan.md](../plan/sentence-args-refactor-plan.md)
- **Tests**: [test_input_to_get_char.cpp](../../tests/test_lpc_interpreter/test_input_to_get_char.cpp)
- **Key Files**: [lib/lpc/object.{h,c}](../../lib/lpc/object.h), [src/simulate.c](../../src/simulate.c), [src/comm.c](../../src/comm.c)

