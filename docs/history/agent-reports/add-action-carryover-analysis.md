# Analysis: Adding Carryover Arguments to add_action()

**Date**: 2026-02-08  
**Branch**: input_to_funcptr  
**Goal**: Extend `add_action()` to accept carryover arguments, similar to `input_to()`

---

## Current Implementation

### LPC Spec (from [docs/efuns/add_action.md](../../docs/efuns/add_action.md))

**Signature**:
```c
void add_action(string | function fun, string | string * cmd, int flag);
```

**Behavior**:
- Binds a command verb to a local function
- When user types the command, the function is called
- **First argument to callback**: command arguments (text after verb, or empty string)
- **Example**:
  ```c
  // Setup:
  add_action("do_climb", "climb");
  
  // User types: "climb wall"
  // Callback: do_climb("wall")  ← first arg is "wall", not "climb wall"
  ```

### Current Data Flow

**1. Efun Wrapper** ([lib/efuns/command.c:189](../../lib/efuns/command.c#L189)):
```c
void f_add_action(void) {
    uint64_t flag = (st_num_arg == 3) ? (sp--)->u.number : 0;
    
    if (sp->type == T_ARRAY) {
        // Handle array of verbs
        for (i = 0; i < n; i++) {
            add_action(sp - 1, sv[i].u.string, flag & 3);
        }
    } else {
        // Single verb
        add_action(sp - 1, sp->u.string, flag & 3);
    }
}
```

**2. Internal `add_action()`** ([src/simulate.c:2009](../../src/simulate.c#L2009)):
```c
void add_action(svalue_t *str, char *cmd, int flag) {
    sentence_t *p = alloc_sentence();
    
    if (str->type == T_STRING) {
        p->function.s = make_shared_string(str->u.string);
        p->flags = flag;
    } else {
        p->function.f = str->u.fp;
        str->u.fp->hdr.ref++;
        p->flags = flag | V_FUNCTION;
    }
    
    p->ob = ob;
    p->verb = make_shared_string(cmd);
    // ⚠️ No args field (not storing carryover args)
    
    p->next = command_giver->sent;
    command_giver->sent = p;
}
```

**3. Command Execution** ([src/simulate.c:1825](../../src/simulate.c#L1825)):
```c
int user_parser(char *buff) {
    // ... find matching sentence by verb ...
    
    for (s = save_command_giver->sent; s; s = s->next) {
        if (/* verb matches */) {
            // Push command args (text after verb)
            if (s->flags & V_NOSPACE)
                copy_and_push_string(&buff[strlen(s->verb)]);
            else if (buff[length] == ' ')
                copy_and_push_string(&buff[length + 1]);
            else
                push_undefined();
            
            // Call function with 1 argument (command args only)
            if (s->flags & V_FUNCTION)
                ret = call_function_pointer(s->function.f, 1);
            else
                ret = apply(s->function.s, s->ob, 1, where);
        }
    }
}
```

### Current Limitations

❌ **No way to pass context to command handlers**  
Example use case that's currently impossible:
```c
// Want to pass player object or context to command handler
void init() {
    object player = this_player();
    add_action("handle_command", "use", 0);  
    // ⚠️ Can't pass 'player' to handle_command()
}

int handle_command(string args) {
    // ⚠️ Must call this_player() again, but it might have changed!
    // ⚠️ Can't access original context from init()
}
```

---

## Proposed Extension

### New LPC Spec

**Extended Signature**:
```c
varargs void add_action(string | function fun, string | string * cmd, int flag, ...);
```

**New Behavior**:
- Additional arguments after `flag` are passed to the callback
- **Callback receives**: `func(cmd_args, carryover_arg1, carryover_arg2, ...)`
- **Order preserved**: Command args come FIRST, then carryover args

**Example**:
```c
// Setup with carryover args:
void init() {
    object player = this_player();
    add_action("do_climb", "climb", 0, player, "context_data");
}

// User types: "climb wall"
// Callback: do_climb("wall", <player_obj>, "context_data")
int do_climb(string args, object who, string context) {
    // args = "wall"
    // who = player object from init()
    // context = "context_data"
}
```

### Backward Compatibility

✅ **100% backward compatible**:
- `add_action("func", "verb")` → same as before, no args
- `add_action("func", "verb", flag)` → same as before, no args
- `add_action("func", "verb", flag, arg1, ...)` → **NEW**, passes args

No existing code breaks because varargs are optional.

---

## Implementation Design

### Changes Required

Since we're already adding `array_t *args` to `sentence_t` for `input_to()`, we can **reuse the same field** for `add_action()` carryover arguments!

#### 1. Update Function Spec ([lib/efuns/func_spec.c](../../lib/efuns/func_spec.c))

**Current**:
```c
void add_action(string | function, string | string *, void | int);
```

**Proposed**:
```c
varargs void add_action(string | function, string | string *, void | int, ...);
```

#### 2. Modify Efun Wrapper ([lib/efuns/command.c](../../lib/efuns/command.c))

**Current** (lines 189-220):
```c
void f_add_action(void) {
    uint64_t flag;
    if (st_num_arg == 3)
        flag = (sp--)->u.number;
    else
        flag = 0;
    
    if (sp->type == T_ARRAY) {
        for (i = 0; i < n; i++) {
            add_action(sp - 1, sv[i].u.string, flag & 3);
        }
    } else {
        add_action(sp - 1, sp->u.string, flag & 3);
    }
}
```

**Proposed**:
```c
void f_add_action(void) {
    uint64_t flag = 0;
    int num_carry = 0;
    svalue_t *carry_args = NULL;
    
    // Extract varargs if present
    if (st_num_arg >= 3) {
        // Check if 3rd arg is a number (flag) or first carryover arg
        if ((sp - (st_num_arg - 3))->type == T_NUMBER) {
            flag = (sp - (st_num_arg - 3))->u.number;
            num_carry = st_num_arg - 3;
            if (num_carry > 0)
                carry_args = sp - num_carry + 1;
        } else {
            // 3rd arg is first carryover, no flag
            flag = 0;
            num_carry = st_num_arg - 2;
            carry_args = sp - num_carry + 1;
        }
    }
    
    if ((sp - (st_num_arg - 2))->type == T_ARRAY) {
        // Handle array of verbs
        int i, n = (sp - (st_num_arg - 2))->u.arr->size;
        svalue_t *sv = (sp - (st_num_arg - 2))->u.arr->item;
        
        for (i = 0; i < n; i++) {
            if (sv[i].type == T_STRING) {
                add_action(sp - (st_num_arg - 1), sv[i].u.string, 
                          flag & 3, num_carry, carry_args);
            }
        }
    } else {
        // Single verb
        add_action(sp - (st_num_arg - 1), 
                  (sp - (st_num_arg - 2))->u.string, 
                  flag & 3, num_carry, carry_args);
    }
    
    // Pop all args
    pop_n_elems(st_num_arg);
}
```

#### 3. Extend Internal `add_action()` ([src/simulate.c](../../src/simulate.c))

**Current signature**:
```c
void add_action(svalue_t *str, char *cmd, int flag);
```

**Proposed signature**:
```c
void add_action(svalue_t *str, char *cmd, int flag, int num_carry, svalue_t *carry_args);
```

**Implementation**:
```c
void add_action(svalue_t *str, char *cmd, int flag, int num_carry, svalue_t *carry_args) {
    sentence_t *p;
    object_t *ob;
    
    if (current_object->flags & O_DESTRUCTED)
        return;
    
    ob = current_object;
#ifndef NO_SHADOWS
    while (ob->shadowing) {
        ob = ob->shadowing;
    }
    if ((ob != current_object) && str->type == T_STRING
        && is_static(str->u.string, ob)) {
        return;
    }
#endif
    
    if (command_giver == 0 || (command_giver->flags & O_DESTRUCTED))
        return;
    
    if (ob != command_giver && ob->super != command_giver &&
        ob->super != command_giver->super && ob != command_giver->super)
        return;
    
    p = alloc_sentence();
    
    if (str->type == T_STRING) {
        p->function.s = make_shared_string(str->u.string);
        p->flags = flag;
    } else {
        p->function.f = str->u.fp;
        str->u.fp->hdr.ref++;
        p->flags = flag | V_FUNCTION;
    }
    
    p->ob = ob;
    p->verb = make_shared_string(cmd);
    
    // ⭐ NEW: Store carryover args
    if (num_carry > 0) {
        array_t *arg_array = allocate_empty_array(num_carry);
        for (int i = 0; i < num_carry; i++)
            assign_svalue_no_free(&arg_array->item[i], &carry_args[i]);
        p->args = arg_array;
    } else {
        p->args = NULL;
    }
    
    p->next = command_giver->sent;
    command_giver->sent = p;
}
```

#### 4. Modify Command Parser ([src/simulate.c](../../src/simulate.c))

**Current** (in `user_parser()`, lines 1933-1950):
```c
// Push command args
if (s->flags & V_NOSPACE)
    copy_and_push_string(&buff[strlen(s->verb)]);
else if (buff[length] == ' ')
    copy_and_push_string(&buff[length + 1]);
else
    push_undefined();

// Call function with 1 arg
if (s->flags & V_FUNCTION)
    ret = call_function_pointer(s->function.f, 1);
else
    ret = apply(s->function.s, s->ob, 1, where);
```

**Proposed**:
```c
// Push command args FIRST (correct LPC order)
if (s->flags & V_NOSPACE)
    copy_and_push_string(&buff[strlen(s->verb)]);
else if (buff[length] == ' ')
    copy_and_push_string(&buff[length + 1]);
else
    push_undefined();

// ⭐ NEW: Push carryover args AFTER command args
int num_args = 1;  // Command args
if (s->args) {
    for (int i = 0; i < s->args->size; i++) {
        push_svalue(&s->args->item[i]);
    }
    num_args += s->args->size;
}

// Call function with all args
if (s->flags & V_FUNCTION)
    ret = call_function_pointer(s->function.f, num_args);
else
    ret = apply(s->function.s, s->ob, num_args, where);
```

---

## Argument Order Verification

**Critical requirement**: Command args must come FIRST, then carryover args.

### Example 1: Basic Usage
```c
// Setup:
add_action("do_use", "use", 0, player_obj, 42);

// User types: "use sword"
// Stack before call: ["sword", player_obj, 42]
// Function signature: int do_use(string args, object who, int context)
// ✅ CORRECT ORDER
```

### Example 2: Empty Command Args
```c
// Setup:
add_action("do_quit", "quit", 0, reason_msg);

// User types: "quit"  (no args)
// Stack before call: [undefined, reason_msg]
// Function signature: int do_quit(string args, string msg)
// args = undefined (or empty string depending on implementation)
// ✅ CORRECT ORDER
```

### Example 3: V_NOSPACE Flag
```c
// Setup:
add_action("do_prefix", "!", 1, context);  // V_NOSPACE flag

// User types: "!help"
// verb = "!"
// command args = "help"
// Stack: ["help", context]
// ✅ CORRECT ORDER
```

---

## Use Cases Enabled

### 1. Passing Player Context
```c
void init() {
    object player = this_player();
    add_action("cmd_sell", "sell", 0, player);
}

int cmd_sell(string args, object seller) {
    // 'seller' is guaranteed to be the player from init()
    // Even if this_player() has changed by execution time
}
```

### 2. Closure-like Behavior
```c
void create() {
    mapping config = ([ "allow_combat": 1, "safe_zone": 0 ]);
    add_action("cmd_attack", "attack", 0, config);
}

int cmd_attack(string args, mapping room_config) {
    if (room_config["safe_zone"]) {
        write("You cannot attack in a safe zone!\n");
        return 1;
    }
    // ... attack logic ...
}
```

### 3. Wizard Tools with Context
```c
void add_debug_commands(object wizard, int debug_level) {
    add_action("cmd_debug", "debug", 0, wizard, debug_level);
    add_action("cmd_trace", "trace", 0, wizard, debug_level);
}

int cmd_debug(string args, object invoker, int level) {
    if (level < 5) {
        write("Debug level too low.\n");
        return 1;
    }
    // ... debug command ...
}
```

### 4. Multi-Object Command Handlers
```c
// In a shop object:
void init() {
    object shop = this_object();
    object vendor = find_living("shopkeeper");
    add_action("cmd_buy", "buy", 0, shop, vendor);
}

int cmd_buy(string args, object shop, object vendor) {
    // Both shop and vendor objects available
    // No need to search for them again
}
```

---

## Compatibility Analysis

### Backward Compatibility

✅ **Existing Code Unchanged**:
```c
// All these still work exactly as before:
add_action("func", "verb");
add_action("func", "verb", 0);
add_action("func", "verb", 1);
add_action("func", ({"verb1", "verb2"}));
add_action((:my_func:), "verb");
```

✅ **No API Breaking**: The varargs are optional, default behavior is preserved.

✅ **No Performance Impact**: If no carryover args, `sentence->args = NULL` (no overhead).

### Edge Cases

#### 1. Array of Verbs + Carryover Args
```c
add_action("handler", ({"get", "take", "pickup"}), 0, context);

// All three verbs will call:
// handler(cmd_args, context)
```
**Implementation**: Loop over verb array, pass same carryover args to each `add_action()` call.

#### 2. Function Pointer + Carryover Args
```c
function f = (:do_climb:);
add_action(f, "climb", 0, player, data);

// Calls: do_climb(cmd_args, player, data)
```
**Note**: Unlike `input_to()` where funptr bound args were problematic due to order, here it's simpler:
- Funptr bound args are ignored (like current behavior)
- Only sentence->args are used
- Same approach as revised input_to() design

#### 3. Remove Action
```c
// Cleanup still works the same way
remove_action("climb", "do_climb");
// Carryover args are freed with the sentence
```

---

## Implementation Scope

### Files to Modify

| File | Changes | Line References |
|------|---------|-----------------|
| [lib/efuns/func_spec.c](../../lib/efuns/func_spec.c) | Add varargs to signature | Line 135 |
| [lib/efuns/command.c](../../lib/efuns/command.c) | Extract and pass carryover args | Lines 189-220 |
| [src/simulate.c](../../src/simulate.c) | Add num_carry/carry_args params to `add_action()` | Lines 2009-2060 |
| [src/simulate.c](../../src/simulate.c) | Push carryover args in `user_parser()` | Lines 1933-1950 |
| [docs/efuns/add_action.md](../../docs/efuns/add_action.md) | Document new varargs feature | Full file |

**Note**: No changes to `sentence_t` needed - we're reusing the `args` field being added for `input_to()`!

**Estimated scope**: ~100 lines modified, ~30 lines added, ~5 lines documentation

---

## Testing Requirements

### Unit Tests

```c
// Test basic carryover
add_action("test_cmd", "test", 0, 42, "arg2");
// User: "test foo"
// Expected: test_cmd("foo", 42, "arg2")

// Test with array of verbs
add_action("handler", ({"a", "b"}), 0, ctx);
// User: "a x" → handler("x", ctx)
// User: "b y" → handler("y", ctx)

// Test with V_NOSPACE
add_action("prefix", "!", 1, data);
// User: "!help" → prefix("help", data)

// Test with function pointer
function f = (:my_func:);
add_action(f, "cmd", 0, arg);
// User: "cmd test" → my_func("test", arg)

// Test with no carryover (backward compat)
add_action("old_style", "old");
// User: "old test" → old_style("test")

// Test cleanup
add_action("tmp", "tmp", 0, obj);
remove_action("tmp");
// Verify args are freed
```

### Integration Tests

```python
# Test with m3_mudlib
send("add_action('test_handler', 'testcmd', 0, 123, 'data');")
send("testcmd arg1 arg2")
expect("handler received: arg1 arg2, extra: 123, data")
```

---

## Risks and Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| Breaking existing mudlibs | High | Varargs are optional, default behavior preserved |
| Performance overhead | Low | Only allocate args array if carryover args provided |
| Memory leaks | Medium | Leverage existing `free_sentence()` cleanup |
| Argument parsing complexity | Medium | Follow `input_to()` pattern, already proven |

---

## Comparison: input_to() vs add_action()

| Feature | `input_to()` | `add_action()` |
|---------|-------------|----------------|
| **Purpose** | Next user input line | Command verb matching |
| **Callback trigger** | Any input line | Specific verb |
| **First arg** | Full input line | Command args (after verb) |
| **Carryover order** | AFTER input | AFTER command args |
| **Storage** | One callback per interactive | Linked list per command_giver |
| **Cleanup** | On next input | On remove_action/object move |

**Both now share**: `sentence_t->args` for carryover arguments! ✅

---

## Recommendation

**Strongly recommend implementing this extension**:

1. ✅ **Architecturally consistent**: Follows same pattern as `input_to()` carryover args
2. ✅ **Reuses infrastructure**: `sentence_t->args` field serves both efuns
3. ✅ **Enables new patterns**: Closure-like behavior, context passing
4. ✅ **100% backward compatible**: Varargs are optional
5. ✅ **Low implementation cost**: ~100 lines of code
6. ✅ **High value**: Solves long-standing limitation in command system

**Implementation order**:
1. Complete `input_to()` refactor (adds `sentence_t->args` field)
2. Extend `add_action()` to use the same field (this proposal)
3. Both benefit from shared infrastructure

**Next steps**:
1. Update func_spec.c signature
2. Modify f_add_action() to extract varargs
3. Update add_action() internal function
4. Modify user_parser() to push carryover args
5. Add unit tests
6. Update documentation
