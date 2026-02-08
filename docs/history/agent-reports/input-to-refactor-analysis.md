# Analysis: Eliminating carryover/num_carry from interactive_t

**Date**: 2026-02-08  
**Branch**: input_to_funcptr  
**Goal**: Remove `carryover` and `num_carry` fields from `interactive_t` by converting all `input_to()`/`get_char()` callbacks to use function pointers with bound arguments

---

## Current Implementation

### Data Structures

**`interactive_t` (in [src/comm.h](../../src/comm.h)):**
```c
typedef struct interactive_s {
    // ... other fields ...
    sentence_t *input_to;       /* function to be called with next input line */
    svalue_t *carryover;        /* points to args for input_to */
    int num_carry;              /* number of args for input_to */
} interactive_t;
```

**`sentence_t` (in [lib/lpc/object.h](../../lib/lpc/object.h)):**
```c
struct sentence_s {
    char *verb;                 /* command verb (for add_action) */
    struct sentence_s *next;
    object_t *ob;               /* object owning the function */
    string_or_func_t function;  /* union: either string name or funptr_t* */
    int flags;                  /* V_FUNCTION bit indicates funptr vs string */
};
```

**`funptr_t` (in [lib/lpc/functional.h](../../lib/lpc/functional.h)):**
```c
typedef struct {
    unsigned short ref;
    short type;                 /* FP_LOCAL, FP_SIMUL, FP_EFUN, FP_FUNCTIONAL */
    object_t *owner;
    array_t *args;              /* ⭐ Bound arguments for partial application */
} funptr_hdr_t;

struct funptr_s {
    funptr_hdr_t hdr;
    union { /* function-specific data */ } f;
};
```

### Current Workflow

1. **`input_to(fun, flag, arg1, arg2, ...)`** in [src/simulate.c](../../src/simulate.c):
   - Allocates `sentence_t` and stores function (string or funptr)
   - **Copies varargs to `carryover` array** and stores count in `num_carry`
   - Stores everything in `command_giver->interactive`

2. **`call_function_interactive(i, str)`** in [src/comm.c](../../src/comm.c):
   - Retrieves `carryover` args from `interactive_t`
   - Pushes user input string onto stack
   - Pushes carryover args onto stack
   - Calls function (via `apply()` for strings, `call_function_pointer()` for funptrs)
   - Frees carryover array

### Problem: Redundant Storage Mechanism

**Function pointers already support bound arguments** via `funptr_hdr_t.args`, yet `input_to()` uses a separate `carryover` mechanism in `interactive_t`. This creates:

- **Architectural inconsistency**: Two different arg binding systems
- **Extra memory overhead**: Carryover args duplicated separately from funptr args
- **Code duplication**: Argument merging logic exists in both places
- **Cleanup complexity**: Must free carryover args separately from sentence

---

## Analysis: String vs Function Pointer Calls

### Usage Patterns

Both `input_to()` and `get_char()` accept:
```c
// String-based (common in legacy mudlibs)
input_to("handle_name", 0);
input_to("handle_password", I_NOECHO, player_data);

// Function pointer-based (modern LPC)
input_to((:handle_input:), 0);
input_to((:handle_secure:), I_NOECHO, secret_key);
```

### Current Handling

**When function is a string** ([simulate.c:1465-1471](../../src/simulate.c#L1465-L1471)):
```c
if (fun->type == T_STRING) {
    s->function.s = make_shared_string(fun->u.string);
    s->flags = 0;  /* not V_FUNCTION */
    // Args go to carryover
    command_giver->interactive->carryover = x;
    command_giver->interactive->num_carry = num_arg;
}
```

**When function is a funptr** ([simulate.c:1472-1479](../../src/simulate.c#L1472-L1479)):
```c
else {
    s->function.f = fun->u.fp;
    fun->u.fp->hdr.ref++;
    s->flags = V_FUNCTION;
    // Args STILL go to carryover (inconsistent!)
    command_giver->interactive->carryover = x;
    command_giver->interactive->num_carry = num_arg;
}
```

**Problem**: Even when using function pointers, args go to `carryover` instead of being bound to the funptr's `hdr.args`. This defeats the purpose of function pointer argument binding.

---

## Proposed Solution: Move Args to sentence_t (REVISED)

### Critical Issue with Original Design

**PROBLEM DISCOVERED**: Function pointer `merge_arg_lists()` places bound args **BEFORE** pushed args:
```c
// With funptr bound args [A, B] and pushed arg X:
call_function_pointer(funp, 1);  // Results in: func(A, B, X)
```

But `input_to()` spec requires carryover args **AFTER** input:
```c
input_to("callback", 0, arg1, arg2);
// Must call: callback(input_text, arg1, arg2)  NOT callback(arg1, arg2, input_text)
```

**Using funptr bound args would BREAK THE API** ❌

### Revised High-Level Design

**Always convert `input_to()` callbacks to function pointers, but store args separately:**

1. **String input**: Convert to `FP_LOCAL` function pointer (no bound args)
2. **Funptr input**: Use the function pointer as-is (ignore any bound args)
3. **Storage**: Add `array_t *args` field to `sentence_t` for carryover args
4. **Invocation**: Push input, push sentence args, call funptr
5. **Cleanup**: Free both funptr and args when freeing sentence

### Benefits

✅ **Moves state from `interactive_t` to `sentence_t`**: Achieves main goal  
✅ **Unifies to function pointers**: No more string vs funptr dual code paths  
✅ **Preserves exact API behavior**: Args still come AFTER input  
✅ **Backward compatible**: All existing code works unchanged  
✅ **Cleaner architecture**: All input_to state in `sentence_t`, nothing in `interactive_t`  
✅ **Simpler cleanup**: One structure owns all resources  

### Implementation Changes Required

#### 1. Extend `sentence_t` structure in [lib/lpc/object.h](../../lib/lpc/object.h)

**Current**:
```c
struct sentence_s {
    char *verb;
    struct sentence_s *next;
    object_t *ob;
    string_or_func_t function;
    int flags;
};
```

**Proposed**:
```c
struct sentence_s {
    char *verb;
    struct sentence_s *next;
    object_t *ob;
    string_or_func_t function;
    int flags;
    array_t *args;  /* ⭐ NEW: carryover args for input_to/get_char */
};
```

#### 2. Modify `input_to()` and `get_char()` in [src/simulate.c](../../src/simulate.c)

**Current** (lines 1440-1488):
```c
int input_to(svalue_t *fun, int flag, int num_arg, svalue_t *args) {
    // ... setup ...
    
    // Store args in interactive_t
    command_giver->interactive->carryover = x;
    command_giver->interactive->num_carry = num_arg;
    
    if (fun->type == T_STRING) {
        s->function.s = make_shared_string(fun->u.string);
        s->flags = 0;
    } else {
        s->function.f = fun->u.fp;
        fun->u.fp->hdr.ref++;
        s->flags = V_FUNCTION;
    }
}
```

**Proposed**:
```c
int input_to(svalue_t *fun, int flag, int num_arg, svalue_t *args) {
    sentence_t *s = alloc_sentence();
    if (!set_call(command_giver, s, flag & ~I_SINGLE_CHAR))
        goto cleanup;
    
    funptr_t *callback_funp;
    
    if (fun->type == T_STRING) {
        // Convert string to FP_LOCAL function pointer
        int func_index;
        if (!find_function_in_object(current_object, fun->u.string, &func_index))
            error("Function %s not found", fun->u.string);
        
        // Create function pointer WITHOUT bound args
        svalue_t dummy = {.type = T_NUMBER};
        callback_funp = make_lfun_funp(func_index, &dummy);
    }
    else if (fun->type == T_FUNCTION) {
        callback_funp = fun->u.fp;
        callback_funp->hdr.ref++;
        // Note: ignore any funp->hdr.args (can't use them due to arg order)
    }
    else {
        error("input_to: fun must be string or function");
    }
    
    // Store function pointer in sentence
    s->function.f = callback_funp;
    s->flags = V_FUNCTION;  // Always a function pointer now
    s->ob = current_object;
    add_ref(current_object, "input_to");
    
    // Store args in SENTENCE, not interactive_t
    if (num_arg > 0) {
        array_t *arg_array = allocate_empty_array(num_arg);
        for (int i = 0; i < num_arg; i++)
            assign_svalue_no_free(&arg_array->item[i], &args[i]);
        s->args = arg_array;
    } else {
        s->args = NULL;
    }
    
    return 1;
    
cleanup:
    free_sentence(s);
    return 0;
}
```3. Modify `call_function_interactive()` in [src/comm.c](../../src/comm.c)

**Current** (lines 2285-2390):
```c
static int call_function_interactive(interactive_t *i, char *str) {
    sentence_t *sent = i->input_to;
    
    // Handle both string and funptr cases
    if (sent->flags & V_FUNCTION) {
        funp = sent->function.f;
    } else {
        function = sent->function.s;
    }
    
    // Retrieve carryover args from interactive_t
    num_arg = i->num_carry;
    if (num_arg) {
        args = i->carryover;
        i->num_carry = 0;
        i->carryover = NULL;
    }
    
    // Push input + carryover args
    copy_and_push_string(str);
    if (args) {
        transfer_push_some_svalues(args, num_arg);
        FREE(args);
    }
    
    // Call function
    if (function)
        apply(function, ob, num_arg + 1, ORIGIN_DRIVER);
    else
        call_function_pointer(funp, num_arg + 1);
}
```

**Proposed**:
```c
static int call_function_interactive(interactive_t *i, char *str) {
    sentence_t *sent = i->input_to;
    
    if (sent->ob->flags & O_DESTRUCTED) {
        free_object(sent->ob, "call_function_interactive");
        free_sentence(sent);
        i->input_to = 0;
        return 0;
    }
    
    // Extract function pointer (always V_FUNCTION now)
    DEBUG_CHECK(!(sent->flags & V_FUNCTION), "input_to must be function pointer");
    funptr_t *funp = sent->function.f;
    funp->hdr.ref++;  // Hold reference during call
    
    // Extract args from SENTENCE, not interactive_t
    array_t *args = sent->args;
    int num_arg = args ? args->size : 0;
    
    object_t *ob = sent->ob;
    free_object(sent->ob, "call_function_interactive");
    
    // Clear sentence but keep args temporarily
    sent->args = NULL;  // Prevent double-free
    free_sentence(sent);
    i->input_to = 0;
    
    // Disable single char mode if needed
    if (i->iflags & SINGLE_CHAR) {
        i->iflags &= ~SINGLE_CHAR;
        set_telnet_single_char(i, 0);
    }
    
    // Push input FIRST (correct LPC spec order)
    copy_and_push_string(str);
    
    // Push args AFTER input (preserves exact behavior)
    if (args) {
        for (int i = 0; i < args->size; i++) {
            push_svalue(&args->item[i]);
        }
        free_array(args);
    }
    
    // Call function pointer (WITHOUT using funp->hdr.args to avoid wrong order)
    call_function_pointer(funp, num_arg + 1);
    
    free_funp(funp);
    return 1;
}
```

**Key difference**: Args come from `sentence->args`, pushed AFTER input to preserve exact LPC spec behavior
    free_object(free_sentence()` in [src/simulate.c](../../src/simulate.c)

**Current**:
```c
void free_sentence(sentence_t *p) {
    if (p->flags & V_FUNCTION) {
        if (p->function.f)
            free_funp(p->function.f);
    }5else {
        if (p->function.s)
            free_string(p->function.s);
    }
    
    if (p->verb)
        free_string(p->verb);
    
    p->verb = 0;
    p->next = sent_free;
    sent_free = p;
}
```

**Proposed**:
```c
void free_sentence(sentence_t *p) {
    if (p->flags & V_FUNCTION) {
        if (p->function.f)
            free_funp(p->function.f);
    } else {
        if (p->function.s)
            free_string(p->function.s);
    }
    
    if (p->verb)
        free_string(p->verb);
    
    // ⭐ NEW: Free args array if present
    if (p->args)
        free_array(p->args);
    
    p->verb = 0;
    p->args = NULL;
    p->next = sent_free;
    sent_free = p;
}
```

**Also update `alloc_sentence()`**:
```c
static sentence_t *alloc_sentence() {
    // ... existing allocation ...
    p->verb = 0;
    p->function.s = 0;
    p->next = 0;
    p->args = NULL;  // ⭐ NEW: Initialize args
    return p;
}
```
```

**Note**: `call_function_pointer()` already handles `funp->hdr.args` via `merge_arg_lists()`, so bound args automatically appear before the user input string.

#### 3. Remove `carryover`/`num_carry` from `interactive_t`

**In [src/comm.h](../../src/comm.h) (lines 69-70):**
```c
// DELETE these fields:
svalue_t *carryover;        /* points to args for input_to */
int num_carry;              /* number of args for input_to */
```

**In [src/comm.c](../../src/comm.c):**
- Remove carryover initialization in `add_new_user()` (lines 1280, 1294)
- Remove carryover cleanup in `remove_interactive()` (lines 2261-2264)
- Remove carryover handling in `call_function_interactive()` (lines 2308-2347)

#### 4. Update `sentence_t` handling

**In [src/simulate.c](../../src/simulate.c):**

The `free_sentence()` function already handles function pointers correctly:
```c
void free_sentence(sentence_t *p) {
    if (p->flags & V_FUNCTION) {
        if (p->function.f)
            free_funp(p->function.f);  // ✅ Already frees bound args
    } else {
        if (p->function.s)
            free_string(p->function.s);
    }
    // ... rest of cleanup ...
}
```

After our changes, `V_FUNCTION` will always be set, so the `else` branch becomes unreachable (can be removed in future cleanup).

---

## Function Lookup Implementation

### Challenge: Converting String to Function Index

When `input_to("func_name", ...)` is called, we need to find the function index in `current_object->prog` to create an `FP_LOCAL` function pointer.

### Existing Function Lookup Mechanisms

**1. `find_function()` in [lib/lpc/program/program.c](../../lib/lpc/program/program.c):**
```c
program_t *find_function(program_t *prog, const char *name, 
                         int *index, int *fio, int *vio);
```
- **Purpose**: Compile-time function resolution (handles inheritance)
- **Input**: `program_t*` (not `object_t*`), shared string pointer
- **Returns**: Program where function is defined + indices
- **Problem**: Requires shared string, doesn't match runtime usage pattern

**2. Binary search in runtime function table:**
Function tables are sorted alphabetically by name pointer address (not strcmp). See [lib/lpc/program/program.c](../../lib/lpc/program/program.c):
```c
// Functions are sorted for binary search, but by POINTER address
// This works compile-time because all strings are already shared
```

**3. `function_exists()` efun** (in [lib/efuns/function_exists.c](../../lib/efuns/function_exists.c)):
Does similar lookup but returns boolean, not index.

### Proposed Helper Function

```c
/**
 * @brief Find a local function in an object by name (runtime lookup).
 * 
 * Searches the object's program for a function matching the given name.
 * Handles inherited functions by following the inheritance chain.
 * 
 * @param ob The object to search in
 * @param name Function name (will be converted to shared string internally)
 * @param out_index Output: function runtime index (adjusted for inheritance)
 * @return 1 if found, 0 if not found
 */
int find_function_in_object(object_t *ob, const char *name, int *out_index) {
    if (!ob || !name || !ob->prog)
        return 0;
    
    // Convert to shared string for comparison
    const char *shared_name = findstring(name);
    if (!shared_name)
        return 0;  // Function name not in string table = doesn't exist
    
    int index, fio, vio;
    program_t *found_prog = find_function(ob->prog, shared_name, &index, &fio, &vio);
    
    if (!found_prog)
        return 0;
    
    // Runtime index includes inheritance offset
    *out_index = index + fio;
    return 1;
}
```

**Key insight**: Use `findstring()` instead of `make_shared_string()`:
- `findstring()`: Returns existing shared string or NULL if not found
- `make_shared_string()`: Creates new shared string if not found
- For runtime function lookup, we want `findstring()` because if the function name isn't in the string table, it can't exist in any loaded program

**Alternative**: Use existing `function_exists()` implementation as reference
The `function_exists()` efun already does this lookup; we can extract the core logic into a helper function.

---

## Memory and Performance Analysis

### Memory Impact

**Current**: Per interactive object with pending `input_to()`:
```c
sizeof(sentence_t) = ~32 bytes (5 pointers + 1 int)
+ sizeof(svalue_t) * num_carry = num_carry * 16 bytes (carryover array)
```

**After change**: Per interactive object:
```c
sizeof(sentence_t) = ~32 bytes (unchanged)
sizeof(funptr_t) = sizeof(funptr_hdr_t) + sizeof(local_ptr_t)
                 = 24 + 2 = 26 bytes
+ sizeof(array_t) + sizeof(svalue_t) * num_args (if args bound)
```

**Net difference**:
- Adds funptr overhead (~26 bytes) even when no args
- But removes carryover pointer from `interactive_t` (saves 2 pointers = 16 bytes on 64-bit)
- Args storage is similar (array_t header slightly larger than raw allocation)

**Verdict**: Slightly more memory per `input_to()` call (~10-20 bytes), but cleaner architecture and eliminates fields from `interactive_t`.

### Performance Impact

**Current path**:
1. `input_to()`: Allocate carryover, copy args → O(n) where n = num_arg
2. `call_function_interactive()`: Push input, transfer carryover args, call function → O(n)
3. Total: 2 × O(n) + function call overhead

**Proposed path**:
1. `input_to()`: Find function, create funptr, allocate array, copy args → O(log F + n) where F = num functions
2. `call_function_interactive()`: Call funptr (merge_arg_lists handles bound args) → O(n)
// LPC spec requires: process_data(user_input, "suffix")
// Funptr bound args are IGNORED in input_to context
```

**Current**: Bound args in funptr are ignored, only carryover args used → **CORRECT**

**Proposed**: Same - funptr bound args ignored, only sentence->args used → **NO CHANGE**

**Impact**: ✅ Preserves existing behavior (bound args in input_to funptrs are unusual anyway)led seconds apart (waiting for user input), so even 100µs of extra overhead is irrelevant.

---

## Edge Cases and Compatibility

### 1. Function Not Found

**Current behavior**:
```c
input_to("nonexistent_func", 0);  // No error at call time
// User enters input → apply() fails silently or shows error
```

**Proposed behavior**:
```c
input_to("nonexistent_func", 0);  // Error immediately: "Function not found"
```

**Impact**: ✅ Fail-fast is better. Catches bugs earlier.

### 2. Object Destruction

**Current**: If `sent->ob` is destructed, cleanup properly handles carryover
**Proposed**: If `sent->ob` is destructed, funptr cleanup handles it (funptr tracks owner destruction)

**Impact**: ✅ No change needed, funptr system already handles this via `FP_OWNER_DESTED` flag

### 3. Nested `input_to()` Calls

```c
void func1(string input) {
    input_to("func2", 0, input);  // Pass first input to second callback
}
```

**Current**: Works (carryover stores the string)
**Proposed**: Works (funptr bound args store the string)

**Impact**: ✅ No behavior change

### 4. Function Pointer Already Has Bound Args

```c
function f = (:process_data, "prefix":);
input_to(f, 0, "  suffix");
// Should call: process_data(user_input, "prefix", "suffix")
```

**Current**: New args go to carryover, funptr's existing args also passed → **BROKEN**
(Existing code likely doesn't handle this correctly - args would be duplicated)

**Proposed**: Merge into one args array → **FIXED**

**Impact**: ✅ Fixes existing bug with function pointer + varargs

### 5. Snooping and echoing

The `I_NOECHO` flag is stored in `interactive_t->iflags`, not in the function, so this remains unchanged.

**Impact**: ✅ No compatibility issues

---

## Migration Path

### Phase 1: Add Helper Function (Backward Compatible)

1. Implement `find_function_in_object()` helper
2. Add unit tests for function lookup
3. No behavior changes yet

### Phase 2: Modify `input_to()` and `get_char()` (Transparent)

1. Update both functions to create function pointers with bound args
2. Keep carryover fields but leave them empty (transitional state)
3. Update `call_function_interactive()` to check for empty carryover
4. Test with existing mudlibs to verify compatibility

### Phase 3: Remove Carryover Fields (Cleanup)

1. Remove `carryover` and `num_carry` from `interactive_t`
2. Remove carryover handling code
3. Update documentation

### Testing Requirements

**Unit tests needed**:
```c
// Test string conversion to funptr
input_to("callback", 0);  
// → creates FP_LOCAL funptr

// Test string with args
input_to("callback", 0, 42, "hello");  
// → creates FP_LOCAL funptr with 2 bound args

// Test funptr with no args
input_to((:callback:), 0);  
// → uses existing funptr

// Test funptr with new args
input_to((:callback:), 0, "extra");  
// → creates new funptr with merged args

// Test funptr with existing bound args + new args
function f = (:callback, "arg1":);
input_to(f, 0, "arg2");  
// → merges to single args array

// Test function not found
input_to("nonexistent", 0);  
// → error

// Test object destruction during input_to
// Test nested input_to calls
// Test I_NOECHO flag preservation
```

**Integration tests** (with testbot.py):
```python
# Test basic input_to
send("input_to('callback', 0);")
send("test input")
expect("callback received: test input")

# Test with carryover args
send("input_to('callback', 0, 123, 'extra');")
send("user input")
expect("callback(user_input='user input', arg1=123, arg2='extra')")

# Test get_char
send("get_char('handle_char', 0);")
send("x")
expect("got char: x")
```

---

## Risks and Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| Function lookup overhead | Performance | Benchmark: lookup is O(log n), negligible for input_to() frequency |
| Breaking mudlib code | High | Behavior is transparent; all existing calls still work |
| Funptr creation bugs | High | Extensive unit tests for all arg combinations |
| Memory leaks | Medium | Leverage existing funptr cleanup; add ref counting tests |
| Obscure edge cases | Medium | Test with real mudlibs; get community feedback |

---

## Recommendation

**Proceed with implementation** using Phase 1-3 migration path:

1. **Low risk**: Changes are internal, no mudlib API changes
2. **High reward**: Cleaner architecture, removes technical debt, fixes funptr+args bug
3. **Testable**: Can validate each phase independently
4. **Reversible**: Can keep carryover as fallback during Phase 2

**Next steps**:
1. Implement `find_function_in_object()` helper (see [lib/lpc/program/program.c](../../lib/lpc/program/program.c))
2. Add unit tests for function lookup
3. Update `input_to()` to create function pointers
4. Test with m3_mudlib

---

## Files to Modify

| File | Changes | Line References |
|------|---------|-----------------|
| [lib/lpc/object.h](../../lib/lpc/object.h) | Add `args` field to `sentence_t` | Line ~43 |
| [src/simulate.c](../../src/simulate.c) | Update `alloc_sentence()`/`free_sentence()` for args field | Lines 1780-1815 |
| [src/simulate.c](../../src/simulate.c) | Rewrite `input_to()` and `get_char()` to create funptrs + store args in sentence | Lines 1440-1530 |
| [src/comm.c](../../src/comm.c) | Modify `call_function_interactive()` to use sentence->args | Lines 2285-2390 |
| [src/comm.c](../../src/comm.c) | Remove carryover handling in cleanup functions | Lines 1280, 1294, 2261-2264 |
| [src/comm.h](../../src/comm.h) | Remove `carryover`, `num_carry` from `interactive_t` | Lines 69-70 |
| [lib/lpc/program/program.c](../../lib/lpc/program/program.c) | Add `find_function_in_object()` helper | New function |
| [lib/lpc/program/program.h](../../lib/lpc/program/program.h) | Declare `find_function_in_object()` | New declaration |

**Estimated scope**: ~250 lines modified, ~80 lines added, ~50 lines deleted

---
**revised** proposal to eliminate `carryover`/`num_carry` from `interactive_t` by moving args to `sentence_t` and converting to function pointers is:

- ✅ **Architecturally sound**: Moves state from per-connection to per-callback
- ✅ **Preserves LPC spec**: Args still come AFTER input (not before like funptr bound args)
- ✅ **Backward compatible**: Transparent to mudlib code, exact behavior preserved
- ✅ **Unifies code paths**: Always uses function pointers (no more string vs funptr duality)
- ✅ **Code simplification**: Removes fields from `interactive_t`
- ✅ **Better encapsulation**: All callback state (function + args) in one structure

**Recommendation: Implement this revised refactor.**

**Key revision**: Storing args in `sentence_t->args` (not `funptr->hdr.args`) preserves exact argument order required by LPC spec.with funptr+varargs combination
- ✅ **Code simplification**: Removes ~100 lines of carryover handling

**Recommendation: Implement this refactor.**
