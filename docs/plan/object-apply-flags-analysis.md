# Analysis: HAS_WRITE_PROMPT and HAS_PROCESS_INPUT Flag Placement

**Date**: 2026-02-11  
**Issue**: These flags are stored on `interactive_t` but represent object program attributes

## Current Implementation

### Flag Definitions
```c
// In comm.h
#define HAS_PROCESS_INPUT   0x0010  /* interactive object has process_input() */
#define HAS_WRITE_PROMPT    0x0020  /* interactive object has write_prompt()  */
```

Stored in: `interactive_t->iflags`

### Usage Pattern

**Initialization** (optimistic):
```c
// backend.c:125 - mudlib_connect()
ob->interactive->iflags |= (HAS_WRITE_PROMPT | HAS_PROCESS_INPUT);

// comm.c:2936 - replace_interactive() [exec()]
ob->interactive->iflags |= (HAS_WRITE_PROMPT | HAS_PROCESS_INPUT);
```

**Checking** (before apply):
```c
// comm.c:2415 - print_prompt()
if (!(ip->iflags & HAS_WRITE_PROMPT))
    tell_object(ip->ob, ip->prompt);
else if (!apply(APPLY_WRITE_PROMPT, ip->ob, 0, ORIGIN_DRIVER))
    ip->iflags &= ~HAS_WRITE_PROMPT;  // Clear if apply fails

// comm.c:1536, 1574 - process_user_command()
if (ip->iflags & HAS_PROCESS_INPUT) {
    ret = apply(APPLY_PROCESS_INPUT, command_giver, 1, ORIGIN_DRIVER);
    if (!ret)
        ip->iflags &= ~HAS_PROCESS_INPUT;  // Clear if apply fails
}
```

### Purpose
Performance optimization: avoid repeated failed `apply()` calls for non-existent functions.

## The Problem

### 1. Conceptual Misplacement

These flags describe **object program attributes** (whether LPC code defines these functions), NOT **connection state**.

**Comparison with existing object flags:**
- `O_ENABLE_COMMANDS` - object can execute commands
- `O_HEART_BEAT` - object has heart_beat() function
- `O_WILL_CLEAN_UP` - object has clean_up() function  
- `O_WILL_RESET` - object has reset() function

All above are on `object_t->flags` because they describe the object, not its interactive state.

### 2. Information Loss

**Scenario:**
1. Object becomes interactive → flags set to `0x0030` (optimistic)
2. `write_prompt()` apply fails → flag cleared to `0x0010`
3. Object becomes non-interactive (destruct or replace)
4. Same object becomes interactive again → flags reset to `0x0030` (optimistic again!)

The cached knowledge that `write_prompt()` doesn't exist is lost.

### 3. Inconsistent Location

| Flag | Location | What It Describes |
|------|----------|-------------------|
| `O_ENABLE_COMMANDS` | `object_t->flags` | Object can execute commands |
| `O_HEART_BEAT` | `object_t->flags` | Object has heart_beat() |
| `HAS_PROCESS_INPUT` | `interactive_t->iflags` | Object has process_input() ⚠️ |
| `HAS_WRITE_PROMPT` | `interactive_t->iflags` | Object has write_prompt() ⚠️ |

The two `HAS_*` flags describe object capabilities but are stored on the connection.

## Evaluation of Solutions

### Option 1: Keep on interactive_t (Status Quo)

**Pros:**
- No structural changes required
- Simple implementation
- Works for the common case (objects stay interactive)

**Cons:**
- ❌ Conceptually incorrect placement
- ❌ Information lost if interactive status toggles
- ❌ Inconsistent with other apply-cache flags
- ❌ Confusing for developers (object attribute stored on connection)

**Verdict**: Pragmatic but conceptually flawed. Works but wrong.

### Option 2: Move to object_t->flags

**Available Space:**
```c
// object.h - Current usage (16-bit field)
#define O_HEART_BEAT            0x0001
#define O_IS_WIZARD             0x0002  // unused
#define O_LISTENER              0x0004  // overlaps with O_ENABLE_COMMANDS
#define O_ENABLE_COMMANDS       0x0004  // overlaps with O_LISTENER
#define O_CLONE                 0x0008
#define O_DESTRUCTED            0x0010
#define O_CONSOLE_USER          0x0020
#define O_ONCE_INTERACTIVE      0x0040
#define O_RESET_STATE           0x0080
#define O_WILL_CLEAN_UP         0x0100
#define O_VIRTUAL               0x0200
#define O_HIDDEN                0x0400
#define O_EFUN_SOCKET           0x0800
#define O_WILL_RESET            0x1000
// Available: 0x2000, 0x4000
#define O_UNUSED                0x8000
```

**Available bits**: 3 unused (0x2000, 0x4000, 0x8000)

**Pros:**
- ✅ Conceptually correct location
- ✅ Persistent across interactive status changes  
- ✅ Consistent with O_HEART_BEAT, O_WILL_CLEAN_UP patterns
- ✅ Fits existing pattern of object capability flags

**Cons:**
- ⚠️ Requires 2 flag bits (available but consuming limited space)
- ⚠️ Flags need invalidation when program changes (not currently implemented)
- ⚠️ Changes object_t structure (affects debugging, maybe binary saves)

**Implementation Requirements:**
1. Define new flags:
   ```c
   #define O_HAS_PROCESS_INPUT 0x2000
   #define O_HAS_WRITE_PROMPT  0x4000
   ```

2. Update all usage sites (6 total)

3. **CRITICAL**: Invalidate flags when program changes
   ```c
   // In reload_object() or similar:
   ob->flags &= ~(O_HAS_PROCESS_INPUT | O_HAS_WRITE_PROMPT);
   ```

**Verdict**: Conceptually correct. Requires invalidation logic for program changes.

### Option 3: Program-Level Cache

Store cached apply existence in `program_t` instead of `object_t`.

**Pros:**
- ✅ Most semantically correct (program defines functions, not objects)
- ✅ Shared across all clones (memory efficient)
- ✅ Automatically invalidated when program recompiled

**Cons:**
- ❌ More complex implementation
- ❌ Requires adding fields to program_t
- ❌ Different pattern from existing apply caches (O_HEART_BEAT, etc.)
- ❌ Affects binary save/load format

**Verdict**: Theoretically best but high implementation cost.

### Option 4: Remove Cache Entirely

Just call `apply()` every time and let the apply lookup handle failures.

**Pros:**
- ✅ Simplest code
- ✅ No flag management needed
- ✅ Always correct

**Cons:**
- ❌ Performance: failed apply lookups happen every prompt/command
- ❌ `write_prompt()` called every prompt (potentially multiple times per second per user)

**Performance Impact**: For 100 users, ~200 failed lookups/second if object lacks functions.

**Verdict**: Clean but wasteful. May not be acceptable for high-user-count scenarios.

## Recommendation

### Immediate (This Refactor)

**Keep on interactive_t with documentation**:
- Add comment explaining the semantic incorrectness
- Note in refactor plan as "known technical debt"
- Reason: comm separation refactor already complex, don't add structural changes

### Future Improvement (Separate Refactor)

**Move to object_t->flags**:
1. Use available bits 0x2000 and 0x4000
2. Implement program change invalidation (affects `reload_object()`)
3. Benefits all objects (even non-interactive ones can cache the info)
4. Aligns with existing patterns

**Alternative**: If expanding `object_t->flags` to `unsigned long` is planned anyway, this becomes even more attractive.

## Impact on Comm Separation Refactor

**For the accessor API**:

Current plan has:
```c
int query_process_input_flag(object_t *ob);
void set_process_input_flag(object_t *ob, int enable);
int query_write_prompt_flag(object_t *ob);
void set_write_prompt_flag(object_t *ob, int enable);
```

**These names are misleading** if flags stay on `interactive_t`:
- They suggest object-level attributes (which is semantically correct)
- But they'll actually access `ob->interactive->iflags`

**Recommendation for Refactor**:

1. **If keeping on interactive_t**, use clearer names:
   ```c
   // Emphasize these are connection-specific caches
   int query_cached_process_input(object_t *ob);
   void set_cached_process_input(object_t *ob, int enable);
   int query_cached_write_prompt(object_t *ob);
   void set_cached_write_prompt(object_t *ob, int enable);
   ```

2. **Or use future-proof names** (assuming eventual move to object_t):
   ```c
   // Use names that work regardless of storage location
   int has_process_input_apply(object_t *ob);
   void cache_process_input_apply(object_t *ob, int exists);
   int has_write_prompt_apply(object_t *ob);
   void cache_write_prompt_apply(object_t *ob, int exists);
   ```

This makes the caching nature explicit and doesn't presume storage location.

## Conclusion

**Current State**: Flags are on `interactive_t` but describe object program attributes - conceptually incorrect but functional.

**For Comm Refactor**: Document this as technical debt, use accessor names that emphasize the caching/connection-specific nature.

**Long-term**: Move to `object_t->flags` with program change invalidation for semantic correctness and consistency.
