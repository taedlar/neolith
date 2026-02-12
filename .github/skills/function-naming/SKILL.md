---
name: function-naming
description: Validates function naming when adding new functions, refactoring existing functions, or reviewing function signatures.
---
# Function Naming Skill

**When to apply**: When adding new functions, refactoring existing functions, or reviewing function signatures.

**Priority**: HIGH - Naming affects API clarity and maintainability across 30+ years of codebase evolution.

## Naming & Style
- **C code**: `snake_case` for functions/variables. No abbreviations; choose unique global names.
- **C++ code**: `CamelCase` for classes/methods. No underscores in test names (GoogleTest restriction).
- **Indentation**: GNU style modified—opening brace on same line as function name for grep-ability.
  ```c
  int my_function(int arg) {  // NOT on new line
      // spaces only, no tabs
  }
  ```
- Return type and function name on same line for searchability.

## Quick Reference Table

| Operation Type | Prefix | Parameter | Example |
|---|---|---|---|
| **Per-object query** | `query_*` | `object_t*` first | `query_prompt(object_t *)` |
| **Per-object setter** | `set_*` | `object_t*` first | `set_prompt(object_t *, const char *)` |
| **Per-object getter** | `get_*` | `object_t*` + output param | `get_pending_command(object_t *, char *)` |
| **Global lookup** | `find_*` or `lookup_*` | search/index param | `lookup_user_at(int)`, `find_object_by_name(const char *)` |
| **Global collection** | `get_all_*` | returns array/collection | `get_all_heart_beats(void)` |
| **Global iterator** | `next_*` | maintains internal state | `next_user_command(void)` |
| **Global check** | descriptive verb | void or simple params | `has_pending_commands(void)`, `grant_all_turns(void)` |
| **Internal helper** | descriptive | any | varies - static only |

## Core Principles

### 1. NO Module Prefixes
❌ **NEVER** use module prefixes like `comm_`, `user_cmd_`, etc.

**Rationale**: Neolith's 30-year-old codebase uses semantic names without prefixes:
- `load_object()`, `destruct_object()` (NOT `simulate_load_object()`)
- `set_heart_beat()`, `query_heart_beat()` (NOT `backend_set_heart_beat()`)

**When you might be tempted**:
- Adding functions to a specific module (still use semantic names)
- Avoiding name conflicts (use more specific semantic names instead)

### 2. Scope Must Be Clear from Signature

**Per-object operations** - ALWAYS take `object_t*` as first parameter:
```c
✅ query_prompt(object_t *ob)
✅ set_prompt(object_t *ob, const char *text)
❌ set_prompt(const char *text)  // Implicit - uses command_giver global
```

**Global operations** - NO `object_t*` parameter:
```c
✅ has_pending_commands(void)      // Checks all users
✅ next_user_command(void)         // Iterator over all users
✅ lookup_user_at(int index)       // Access global array
❌ query_pending_commands(void)    // Ambiguous - query_ implies per-object
```

### 3. Distinguish Iterator from Collection

**Iterators** (`next_*`) - Mutate internal state:
```c
✅ next_user_command(void)         // Advances static counter
✅ next_reset_object(void)         // Walks reset queue
```

**Collections** (`get_all_*`) - Return snapshot, no side effects:
```c
✅ get_all_heart_beats(void)       // Returns array of all heart beat objects
✅ get_all_users(void)             // Returns array of all interactive objects
❌ get_heart_beats(void)           // Ambiguous - single or multiple?
```

### 4. Query vs. Get vs. Set

**Use `query_*`** for simple attribute retrieval (per-object):
```c
✅ query_prompt(object_t *ob)      // Returns const char*
✅ query_idle(object_t *ob)        // Returns time_t
```

**Use `get_*`** when output parameter needed (per-object):
```c
✅ get_pending_command(object_t *ob, char *buf)  // Fills buffer
✅ get_environment(object_t *ob, object_t **env) // Output via pointer
```

**Use `set_*`** for attribute modification (per-object):
```c
✅ set_prompt(object_t *ob, const char *text)
✅ set_heart_beat(object_t *ob, int flag)
```

**Special case - `get_all_*`** for global collections:
```c
✅ get_all_heart_beats(void)       // Returns array
```

## Validation Checklist

When adding or modifying a function, verify:

### ✅ Scope Validation
- [ ] If function operates on specific object → `object_t*` first parameter
- [ ] If function operates globally → NO `object_t*` parameter
- [ ] Name prefix matches scope (`query_`/`set_`/`get_*` = per-object, `find_`/`lookup_`/`next_`/`get_all_*` = global)

### ✅ Prefix Validation
- [ ] NO module prefix (`comm_`, `backend_`, `simulate_`, etc.)
- [ ] Prefix matches operation type (see table above)
- [ ] If returning collection, uses `get_all_*` not just `get_*`
- [ ] If maintaining state, uses `next_*` not `get_*`

### ✅ Signature Validation
- [ ] Per-object functions: `object_t*` is first parameter
- [ ] Setters: All arguments explicit (no implicit `command_giver`)
- [ ] Getters with buffers: Use `get_*` prefix and output parameter
- [ ] Queries: Use `query_*` prefix and return value directly

### ✅ Consistency Validation
- [ ] Check if similar function exists with different pattern
- [ ] Ensure new function aligns with existing conventions
- [ ] If breaking existing pattern, document why in commit message

## Common Mistakes

### ❌ Mistake 1: Module Prefix
```c
❌ comm_set_prompt(object_t *ob, const char *text)
✅ set_prompt(object_t *ob, const char *text)
```

### ❌ Mistake 2: Implicit Command Giver
```c
❌ set_prompt(const char *text)  // Uses command_giver internally
✅ set_prompt(object_t *ob, const char *text)

// Efun wrapper adds convenience:
void f_set_prompt(void) {
    set_prompt(command_giver, sp->u.string);  // Explicit
}
```

### ❌ Mistake 3: Ambiguous Global vs Per-Object
```c
❌ query_pending_commands(void)  // Returns what? For whom?
✅ query_buffered_commands(object_t *ob)  // Per-object
✅ has_pending_commands(void)             // Global check
```

### ❌ Mistake 4: Iterator Named as Simple Getter
```c
❌ get_user_command(void)  // Surprise! Advances internal counter
✅ next_user_command(void) // Clear it's an iterator
```

### ❌ Mistake 5: Collection Named Ambiguously
```c
❌ get_heart_beats(void)      // One or many?
✅ get_all_heart_beats(void)  // Clearly returns all
```

## Examples from Neolith Codebase

### Per-Object Operations (simulate.c, comm.c)
```c
// Queries - return values directly
const char* query_ip_name(object_t *ob);
time_t query_idle(object_t *ob);
object_t* query_snoop(object_t *ob);

// Setters - explicit object parameter
void set_heart_beat(object_t *ob, int flag);
void set_prompt(object_t *ob, const char *text);

// Getters - use output parameters
int get_pending_command(object_t *ob, char *buf);
```

### Global Operations (backend.c, comm.c)
```c
// Collections - return arrays
object_t** get_all_heart_beats(void);

// Iterators - maintain state
char* next_user_command(void);
object_t* next_reset_object(void);

// Lookups - search by key
object_t* lookup_user_at(int index);
object_t* find_object_by_name(const char *name);

// Checks - descriptive verbs
int has_pending_commands(void);
void grant_all_turns(void);
```

### Apply Cache Accessors (Special Case)
```c
// These cache whether object has specific applies
// Named to emphasize behavior, not storage location
int has_process_input_apply(object_t *ob);
void cache_process_input_apply(object_t *ob, int exists);
int has_write_prompt_apply(object_t *ob);
void cache_write_prompt_apply(object_t *ob, int exists);
```

**Rationale**: These describe object program attributes but are currently stored on `interactive_t` (technical debt). Names emphasize caching behavior so they work regardless of future storage location. See [docs/plan/object-apply-flags-analysis.md](../../docs/plan/object-apply-flags-analysis.md).

## Refactoring Guidance

### When Changing Existing Functions

**Implicit → Explicit signatures**:
1. Change function signature to take `object_t*` first
2. Update all call sites to pass explicit object
3. Update efun wrapper to pass `command_giver`:
```c
// Old efun implementation
void f_set_prompt(void) {
    set_prompt(sp->u.string);  // Implicit command_giver
}

// New efun implementation
void f_set_prompt(void) {
    set_prompt(command_giver, sp->u.string);  // Explicit
}
```

**Renaming for clarity**:
- `get_user_command()` → `next_user_command()` (indicates iteration)
- `get_heart_beats()` → `get_all_heart_beats()` (indicates collection)

### When Adding New Module

**DO**:
- Use semantic names based on operation type
- Follow table above for prefix selection
- Make scope clear from signature

**DON'T**:
- Add module prefix to function names
- Create parallel implicit/explicit APIs
- Use `get_*` for both collections and per-object operations

## Integration with Development Workflow

### Code Review Checks
1. Run through validation checklist above
2. Compare with existing similar functions
3. Check for module prefix violations
4. Verify scope matches signature

### Testing
When testing new functions:
1. Verify behavior matches name (no surprises)
2. Test both per-object and global behavior as appropriate
3. Check iterator state management if using `next_*`
