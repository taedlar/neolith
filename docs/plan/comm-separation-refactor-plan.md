# Communication Layer Separation Refactoring Plan

**Goal**: Separate comm.c into stream processing (comm.c) and command dispatch (user_command.c) using object-as-primary-interface design (Option 1), with `interactive_t` becoming opaque to the command layer.

**Date**: 2026-02-08  
**Update**: Merged input_to/add_action refactor into prerequisite plan

## Prerequisites

This refactor depends on completing the sentence-args refactor because:
1. Removes `carryover`/`num_carry` from `interactive_t` (no accessor functions needed)
2. Moves `input_to()`/`get_char()` logic to use `sentence_t->args` (cleaner code to move)
3. Validates argument handling before architectural split (reduces risk)

## Naming Conventions

**CRITICAL**: All function naming must follow established patterns. See [.github/skills/function-naming/SKILL.md](../../.github/skills/function-naming/SKILL.md) for:
- Quick reference table with 8 operation type patterns
- Validation checklist for adding/refactoring functions
- Common mistakes and how to avoid them
- Complete examples from codebase

**Key Requirements for This Refactor**:
- ❌ NO module prefixes (`comm_`, `user_cmd_`, etc.)
- ✅ Per-object operations take `object_t*` as first parameter
- ✅ Use `query_*` for per-object queries, `set_*` for per-object setters
- ✅ Use `find_*`/`lookup_*` for global lookups, `get_all_*` for collections, `next_*` for iterators
- ✅ Setters changed from implicit (`command_giver` global) to explicit (`object_t*` parameter)

**Function Examples from This Refactor**:

Per-object operations:
- `query_buffered_commands(object_t *)` - Check if object has pending commands
- `get_pending_command(object_t *, char *)` - Extract command for specific object
- `consume_command(object_t *)` - Mark command as processed for object
- `set_prompt(object_t *, const char *)` - Set prompt for object (changed from implicit)

Global operations:
- `next_user_command(void)` - Iterator advancing round-robin position
- `has_pending_commands(void)` - Check if any user has pending commands
- `lookup_user_at(int)` - Access user by array index
- `grant_all_turns(void)` - Grant command turn to all users

**Recommended Renames for Existing Functions**:

For consistency with established conventions (see [function naming skill](../../.github/skills/function-naming/SKILL.md#refactoring-guidance)), rename these existing functions:

1. **backend.c**:
   - `get_heart_beats()` → `get_all_heart_beats()` - clarifies it returns all heart beat objects
   - Rationale: `get_all_*` pattern makes global collection nature explicit

2. **comm.c** (internal):
   - `get_user_command()` → `next_user_command()` - clarifies it's an iterator with state
   - Rationale: `next_*` pattern indicates state mutation (advances static counter)

These renames should be done as part of this refactoring work to establish consistent patterns.

## Architecture Overview

### Current State
```
comm.c (3000 lines)
├── Stream I/O (sockets, TELNET protocol, buffering)
├── Command buffer management (text parsing, extraction)
├── Command dispatch (round-robin, input_to callbacks)
└── User state management (prompt, snoop, notify_fail)
```

### Target State
```
comm.c (Stream Layer, ~1800 lines)
├── Socket I/O and async runtime integration
├── TELNET protocol state machine
├── Message buffering and flushing
├── Command buffer management
└── Public API: All functions take object_t*

user_command.c (Command Layer, ~1200 lines)
├── Command dispatch and round-robin scheduling
├── input_to() callback handling
├── process_input() apply calls
├── Prompt and notify_fail management
└── Accesses interactive_t via comm.c API only
```

## Critical Dependencies

### 1. Backend Integration

**Current Flow** (backend.c lines 290-340):
```c
// Grant command turns to all users
for (i = 0; i < max_users; i++) {
    if (all_users[i])
        all_users[i]->iflags |= HAS_CMD_TURN;  // Direct access
}

// Poll for I/O events
nb = do_comm_polling(&timeout);

// Process I/O events (buffering, TELNET)
if (nb > 0)
    process_io();

// Process user commands (round-robin, bounded)
for (i = 0; process_user_command() && i < connected_users; i++);
```

**Key Observations**:
- Backend directly manipulates `all_users[i]->iflags` for turn granting
- `process_io()` must run **before** `process_user_command()`
- `process_user_command()` returns 1 if command processed, 0 if none pending
- Loop bounded by `connected_users` for safety (prevents infinite loops if error)

### 2. Command Turn Design

**Turn Granting** (backend.c):
```c
all_users[i]->iflags |= HAS_CMD_TURN;  // Grant turn
```

**Turn Consumption** (comm.c lines 1958-1960):
```c
if (ip->iflags & HAS_CMD_TURN) {
    ip->iflags &= ~HAS_CMD_TURN;  // Consume turn
    break;  // Process this command
}
```

**Purpose**: Fair round-robin scheduling - each user gets exactly one command processed per backend cycle, prevents command monopolization.

### 3. Protocol Callbacks with Error Handling

**Mudlib Callbacks in Stream Layer** (comm.c):
```c
// Line 670: Terminal type notification
copy_and_push_string(ip->sb_buf + 2);
apply(APPLY_TERMINAL_TYPE, ip->ob, 1, ORIGIN_DRIVER);

// Line 683: Window size notification
push_number(w);
push_number(h);
apply(APPLY_WINDOW_SIZE, ip->ob, 2, ORIGIN_DRIVER);

// Line 777: Generic TELNET suboption
copy_and_push_string(ip->sb_buf);
apply(APPLY_TELNET_SUBOPTION, ip->ob, 1, ORIGIN_DRIVER);
```

❌ **Current Problem**: No validity check after `apply()` calls!

**Risk**: Mudlib code can:
- Call `destruct(this_object())` during callback
- Trigger errors causing LONGJMP
- Invalidate `ip->ob` or `ip` itself

**Required Pattern** (from process_user_command):
```c
apply(APPLY_PROCESS_INPUT, command_giver, 1, ORIGIN_DRIVER);
VALIDATE_IP(ip, command_giver);  // Check if still valid
if (!ret)
    ip->iflags &= ~HAS_PROCESS_INPUT;  // Clear flag if failed
```

### 4. Making interactive_t Opaque

**Phases**:
1. **Phase 1**: Keep `interactive_t` in comm.h (shared), both modules access directly
2. **Phase 2**: Create accessor functions, user_command.c uses accessors only
3. **Phase 3**: Move `interactive_t` definition to comm.c (opaque pointer in user_command.c)

**Required Accessors** (for user_command.c):
```c
// Command state (for input_to/get_char moved to user_command.c)
sentence_t* query_input_to(object_t *ob);
void set_input_to(object_t *ob, sentence_t *sent);
void clear_input_to(object_t *ob);
// NOTE: No carryover accessors needed after sentence-args refactor completes

// Prompt and notify_fail
const char* query_prompt(object_t *ob);
void set_prompt(object_t *ob, const char *prompt);  // Changed from implicit to explicit
string_or_func_t query_notify_fail_value(object_t *ob);
int query_notify_fail_type(object_t *ob);  // 0=string, 1=function
void set_notify_fail_message(object_t *ob, const char *msg);  // Changed from implicit to explicit
void set_notify_fail_function(object_t *ob, funptr_t *fp);  // Changed from implicit to explicit
void clear_notify_fail(object_t *ob);

// Command flags (apply caches - see docs/plan/object-apply-flags-analysis.md)
// NOTE: These cache whether object has process_input()/write_prompt() applies.
// Currently stored on interactive_t (technical debt) but semantically are object attributes.
// Named to emphasize caching behavior rather than storage location for future-proofing.
int has_process_input_apply(object_t *ob);
void cache_process_input_apply(object_t *ob, int exists);
int has_write_prompt_apply(object_t *ob);
void cache_write_prompt_apply(object_t *ob, int exists);
int query_noesc_flag(object_t *ob);
void set_noesc_flag(object_t *ob, int enable);

// Timing
time_t query_last_command_time(object_t *ob);
void update_last_command_time(object_t *ob);

// Snoop
object_t* query_snoop_by(object_t *ob);
object_t* query_snoop_on(object_t *ob);

// OLD_ED support (feature enabled in project)
#ifdef OLD_ED
struct ed_buffer_s* query_ed_buffer(object_t *ob);
void set_ed_buffer(object_t *ob, struct ed_buffer_s *buf);
int query_net_dead(object_t *ob);
#endif

// Efuns using interactive fields
int query_local_port(object_t *ob);         // F_QUERY_IP_PORT
const char* query_peer_ip(object_t *ob);    // For socket_status()
int query_peer_port(object_t *ob);          // For socket_status()
```

## Refactoring Phases

### Phase 1: Establish Module Boundaries (2-3 days)

**Goal**: Create user_command.c and move command dispatch code with minimal changes.

**Decision Record**:
- ✅ `input_to()` and `get_char()` moved from simulate.c to user_command.c (command-layer concerns)
- ✅ F_TRACE will NOT be defined (no trace accessors needed)
- ✅ OLD_ED will be defined (ed_buffer accessors required)
- ✅ After sentence-args refactor: No `carryover`/`num_carry` fields (no accessors needed)

**Steps**:

1. **Create new files**:
   - `src/user_command.c`
   - `src/user_command.h`

2. **Move functions to user_command.c**:
   ```c
   // Main entry point
   int process_user_command(void);

   // Command extraction and scheduling
   static char* next_user_command(void);  // Iterator - advances internal cursor
   
   // input_to/get_char handling
   int input_to(svalue_t *fun, int flag, int num_arg, svalue_t *args);  // FROM simulate.c
   int get_char(svalue_t *fun, int flag, int num_arg, svalue_t *args);  // FROM simulate.c
   static int call_function_interactive(interactive_t *i, char *str);
   int set_call(objec (Stream Layer):
   ```c
   // Stream I/O
   static void get_user_data(interactive_t *ip, io_event_t *evt);
   static size_t copy_chars(...);  // TELNET protocol
   void add_message(object_t *who, char *data);
   void add_vmessage(object_t *who, char *format, ...);
   int flush_message(interactive_t *ip);
   
   // Buffer management
   static char* first_cmd_in_buf(interactive_t *ip);
   static int cmd_in_buf(interactive_t *ip);
   static void next_cmd_in_buf(interactive_t *ip);
   
   // Connection lifecycle
   void new_interactive(socket_fd_t fd);
   void remove_interactive(object_t *ob, int dested);
   void process_io(void);
   int do_comm_polling(struct timeval *timeout);
   
   // Protocol mode control
   static void set_telnet_single_char(interactive_t *ip, int single);
   
   // Network helpers (may refactor to use accessors internally)
   char* query_ip_name(object_t *ob);
   char* query_ip_number(object_t *ob);
   time_t query_idle(object_t *ob);
   ```

5. **Update CMakeLists.txt**:
   ```cmake
   # src/CMakeLists.txt
   add_library(stem OBJECT
       # ... existing files
       comm.c
       user_command.c
   )
   ```

6. **Update exports**:
   - Move declarations from `comm.h` to `user_command.h` as appropriate
   - Add `input_to()` and `get_char()` declarations to `user_command.h`
   - Update `simulate.h` to forward-declare or include `user_command.h`

**Validation**:
- Build succeeds on all platforms
- All existing tests pass
- No behavioral changes
- Line count: comm.c ~1800, user_command.c ~1200, simulate.c reduced by ~150 lines
- Verify `input_to()` and `get_char()` still work from mudlib (already tested in prerequisite)
- Verify `add_action()` with carryover args works (new capability from prerequisite)

### Phase 2: Fix Protocol Error Handling (1-2 days)

**Goal**: Add VALIDATE_IP checks after all mudlib apply() calls in stream layer.

**Steps**:

1. **Wrap protocol callbacks** (in comm.c):
   ```c
   static int safe_protocol_callback(interactive_t *ip, 
                                      const char *apply_name, 
                                      int num_args) {
       object_t *ob = ip->ob;
       if (!ob || (ob->flags & O_DESTRUCTED) || ob->interactive != ip)
           return 0;  // Already invalid
       
       apply(apply_name, ob, num_args, ORIGIN_DRIVER);
       
       // Check if still valid after mudlib code
       if (!ob || (ob->flags & O_DESTRUCTED) || ob->interactive != ip) {
           opt_trace(TT_COMM, "Object destructed during %s callback\n", 
                     apply_name);
           return 0;  // Invalidated by callback
       }
       return 1;  // Still valid
   }
   ```

2. **Update callback sites**:
   ```c
   // Line 670: Terminal type
   copy_and_push_string(ip->sb_buf + 2);
   if (!safe_protocol_callback(ip, APPLY_TERMINAL_TYPE, 1))
       return processed_bytes;  // Abort processing this buffer
   
   // Line 683: Window size
   push_number(w);
   push_number(h);
   if (!safe_protocol_callback(ip, APPLY_WINDOW_SIZE, 2))
       return processed_bytes;
   
   // Line 777: TELNET suboption
   copy_and_push_string(ip->sb_buf);
   if (!safe_protocol_callback(ip, APPLY_TELNET_SUBOPTION, 1))
       return processed_bytes;
   ```

3. **Handle early termination**:
   - `copy_chars()` returns number of bytes processed
   - Caller must handle partial processing gracefully

**Validation**:
- Create test: mudlib calls `destruct(this_object())` in terminal_type()
- Verify no crash, connection properly cleaned up
- Check with Valgrind/ASan for memory leaks

### Phase 3: Backend Integration with Turn Accessors (2-3 days)

**Goal**: Abstract turn management through comm.c API.

**Steps**:

1. **Add turn management API** (comm.c):
   ```c
   /**
    * @brief Grant command processing turn to all connected users.
    * Should be called once per backend cycle before process_user_command().
    * @returns Number of users granted turns.
    */
   int grant_all_turns(void) {
       int count = 0;
       for (int i = 0; i < max_users; i++) {
           if (all_users[i]) {
               all_users[i]->iflags |= HAS_CMD_TURN;
               count++;
           }
       }
       return count;
   }
   
   /**
    * @brief Check if any users have pending commands.
    * Used to optimize backend polling timeout.
    * @returns 1 if commands pending, 0 otherwise.
    */
   int has_pending_commands(void) {
       for (int i = 0; i < max_users; i++) {
           if (all_users[i] && (all_users[i]->iflags & CMD_IN_BUF))
               return 1;
       }
       return 0;
   }
   ```

2. **Update backend.c**:
   ```c
   // Old:
   // for (i = 0; i < max_users; i++) {
   //     if (all_users[i]) {
   //         all_users[i]->iflags |= HAS_CMD_TURN;
   //         connected_users++;
   //         if (!has_pending && (all_users[i]->iflags & CMD_IN_BUF))
   //             has_pending = 1;
   //     }
   // }
   
   // New:
   connected_users = grant_all_turns();
   has_pending_commands = has_pending_commands();
   
   if (heart_beat_flag || has_pending_commands) {
       timeout.tv_sec = 0;
       timeout.tv_usec = 0;
   } else {
       timeout.tv_sec = 60;
       timeout.tv_usec = 0;
   }
   ```

3. **Hide all_users exposure**:
   - Remove `extern interactive_t **all_users;` from comm.h
   - Make `all_users` static to comm.c
   - Provide lookup function: `object_t* lookup_user_at(int index)`
   - Alternative: iterator pattern `foreach_user(callback_fn, context)` if needed

**Validation**:
- Backend still functions correctly
- Round-robin fairness preserved
- Timeout optimization works (verify with trace logs)

### Phase 4: Command Buffer API Interface (3-4 days)

**Goal**: Create clean API for command extraction, hide buffer details.

**Steps**:

1. **Define command buffer API** (comm.h):
   ```c
   /**
    * @brief Get next pending command for an object.
    * Checks for command turn and buffer state.
    * @param ob Interactive object to check.
    * @param[out] command_buf Buffer to receive command (MAX_TEXT size).
    * @returns 1 if command retrieved, 0 if no command pending or no turn.
    */
   int get_pending_command(object_t *ob, char *command_buf);
   
   /**
    * @brief Mark that user's command has been consumed.
    * Advances buffer pointers and clears CMD_IN_BUF if no more commands.
    * @param ob Interactive object.
    */
   void consume_command(object_t *ob);
   
   /**
    * @brief Check if object has pending buffered commands.
    * @param ob Interactive object.
    * @returns 1 if commands available, 0 otherwise.
    */
   int query_buffered_commands(object_t *ob);
   ```

2. **Implement in comm.c**:
   ```c
   int get_pending_command(object_t *ob, char *command_buf) {
       interactive_t *ip;
       char *cmd;
       
       if (!ob || !ob->interactive)
           return 0;
       
       ip = ob->interactive;
       
       // Check turn
       if (!(ip->iflags & HAS_CMD_TURN))
           return 0;
       
       // Check buffer
       if (!(ip->iflags & CMD_IN_BUF))
           return 0;
       
       // Extract command
       cmd = first_cmd_in_buf(ip);
       if (!cmd)
           return 0;
       
       // Copy to output buffer (TELNET negotiation handled)
       telnet_neg(command_buf, cmd);
       
       // Consume turn
       ip->iflags &= ~HAS_CMD_TURN;
       
       return 1;
   }
   
   void consume_command(object_t *ob) {
       interactive_t *ip;
       
       if (!ob || !ob->interactive)
           return;
       
       ip = ob->interactive;
       
       // Advance to next command
       next_cmd_in_buf(ip);
       
       // Clear flag if no more commands
       if (!cmd_in_buf(ip))
           ip->iflags &= ~CMD_IN_BUF;
       
       // Handle NOECHO restoration
       if (ip->iflags & NOECHO) {
           if (ip->fd == STDIN_FILENO) {
               #ifdef HAVE_TERMIOS_H
               struct termios tty;
               tcgetattr(ip->fd, &tty);
               tty.c_lflag |= ECHO;
               safe_tcsetattr(ip->fd, &tty);
               #endif
           } else {
               add_message(ob, telnet_no_echo);
           }
           ip->iflags &= ~NOECHO;
       }
       
       // Update command timestamp
       ip->last_time = current_time;
   }
   ```

3. **Refactor next_user_command()** (user_command.c):
   ```c
   static char* next_user_command(void) {
       static int s_next_user = 0;
       static char buf[MAX_TEXT];
       interactive_t *ip = NULL;
       object_t *ob = NULL;
       int i;
       
       // Round-robin search
       for (i = 0; i < max_users; i++) {
           // Lookup next user in global array
           ob = lookup_user_at(s_next_user);
           
           if (ob) {
               // Flush output first
               flush_output(ob);
               
               // Try to get command
               if (get_pending_command(ob, buf)) {
                   command_giver = ob;
                   if (s_next_user-- == 0)
                       s_next_user = max_users - 1;
                   return buf;
               }
           }
           
           if (s_next_user-- == 0)
               s_next_user = max_users - 1;
       }
       
       return NULL;
   }
   ```

**Validation**:
- Command extraction still works
- TELNET negotiation preserved
- NOECHO restoration works
- Round-robin fairness maintained

### Phase 5: Protocol Mode Abstraction (2-3 days)

**Goal**: Abstract terminal mode changes (echo, single-char) behind clean API.

**Steps**:

1. **Define protocol mode API** (comm.h):
   ```c
   typedef enum {
       INPUT_MODE_NORMAL = 0,      // Line mode with echo
       INPUT_MODE_NOECHO = 1,      // Line mode without echo (passwords)
       INPUT_MODE_CHAR = 2         // Single character mode
   } input_mode_t;
   
   /**
    * @brief Set terminal input mode for interactive object.
    * Handles both TELNET negotiation and console termios.
    * @param ob Interactive object.
    * @param mode Desired input mode.
    */
   void set_input_mode(object_t *ob, input_mode_t mode);
   
   /**
    * @brief Get current input mode.
    * @param ob Interactive object.
    * @returns Current mode, or -1 if not interactive.
    */
   input_mode_t query_input_mode(object_t *ob);
   ```

2. **Implement in comm.c**:
   ```c
   void set_input_mode(object_t *ob, input_mode_t mode) {
       interactive_t *ip;
       
       if (!ob || !ob->interactive)
           return;
       
       ip = ob->interactive;
       
       switch (mode) {
       case INPUT_MODE_NOECHO:
           ip->iflags |= NOECHO;
           if (ip->fd == STDIN_FILENO) {
               #ifdef HAVE_TERMIOS_H
               struct termios tio;
               tcgetattr(ip->fd, &tio);
               tio.c_lflag &= ~ECHO;
               safe_tcsetattr(ip->fd, &tio);
               #endif
           } else {
               add_message(ob, telnet_yes_echo);
           }
           break;
           
       case INPUT_MODE_CHAR:
           ip->iflags |= SINGLE_CHAR;
           set_telnet_single_char(ip, 1);
           break;
           
       case INPUT_MODE_NORMAL:
       default:
           ip->iflags &= ~(NOECHO | SINGLE_CHAR);
           set_telnet_single_char(ip, 0);
           // Echo restoration handled in consume_command()
           break;
       }
   }
   ```

3. **Update set_call()** (user_command.c):
   ```c
   int set_call(object_t *ob, sentence_t *sent, int flags) {
       if (!ob || !sent || !ob->interactive || 
           query_input_to(ob))  // Already has input_to
           return 0;
       
       // Set callback state
       set_input_to(ob, sent);
       
       // Handle input mode flags
       if (flags & I_SINGLE_CHAR)
           set_input_mode(ob, INPUT_MODE_CHAR);
       else if (flags & I_NOECHO)
           set_input_mode(ob, INPUT_MODE_NOECHO);
       
       // Store NOESC flag (command layer concern)
       if (flags & I_NOESC) {
           set_noesc_flag(ob, 1);
       }
       
       return 1;
   }
   ```

**Validation**:
- input_to() with NOECHO still works
- get_char() single character mode works
- Console and TELNET both work correctly

### Phase 6: Create Accessor Functions (3-4 days)

**Goal**: Implement all accessors for interactive_t fields needed by user_command.c.

**Note**: Reduced scope - no carryover accessors needed (eliminated in prerequisite refactor)

**Steps**:

1. **Implement all accessors listed in "Making interactive_t Opaque" section**

2. **Update user_command.c** to use only accessors:
   ```c
   // Old:
   if (ip->input_to) { ... }
   
   // New:
   if (query_input_to(ob)) { ... }
   ```

3. **Systematic conversion**:
   - Search user_command.c for `ip->` patterns
   - Replace with accessor calls
   - Ensure all direct field accesses are via stream layer API

**Validation**:
- Grep `user_command.c` for `ip->` - should find none (all via accessors)
- All functionality still works
- No performance regression (accessors should inline well)

### Phase 7: Make interactive_t Opaque (1-2 days)

**Goal**: Move `interactive_t` definition to comm.c, expose only forward declaration.

**Steps**:

1. **Move definition**:
   ```c
   // comm.h - Remove full definition, keep only:
   typedef struct interactive_s interactive_t;  // Opaque pointer
   
   // comm.c - Move full definition here:
   struct interactive_s {
       object_t *ob;
       socket_fd_t fd;
       char text[MAX_TEXT];
       // ... all fields
   };
   ```

2. **Update headers**:
   - Ensure user_command.c only includes user_command.h
   - No direct includes of comm implementation details

3. **Fix compilation**:
   - May need to expose some constants (MAX_TEXT, flag values)
   - Use `#define` in comm.h for public constants
   - Keep struct definition private

**Validation**:
- Build succeeds: `user_command.c` cannot access `ip->` fields
- Binary compatibility maintained
- Tests pass

## Testing Strategy

### Unit Tests

1. **test_stream_command_separation**:
   ```cpp
   TEST(StreamCommandSeparation, InputToNoEcho) {
       // Verify input_to() with NOECHO flag works
   }
   
   TEST(StreamCommandSeparation, GetCharSingleCharMode) {
       // Verify get_char() character mode works
   }
   
   TEST(StreamCommandSeparation, BufferAPIRoundRobin) {
       // Multiple users, verify round-robin
   }
   
   #ifdef OLD_ED
   TEST(StreamCommandSeparation, EdBufferAccessors) {
       // Verify ed_buffer get/set works
   }
   #endif
   
   TEST(StreamCommandSeparation, CommandTurnFairness) {
       // Grant turns, verify each user gets one
   }
   
   TEST(StreamCommandSeparation, ProtocolCallbackSafetyDestruct) {
       // Mudlib destructs during terminal_type callback
   }
   ```

2. **Update existing tests**:
   - `test_backend/` - Verify command processing loop
   - `test_efuns/test_input_to/` - Verify `input_to()` and `get_char()` still work  
   - `test_simul_efuns/` - Verify no breakage from simulate.c changes

**Note**: input_to() and get_char() test coverage already extensive from sentence-args refactor prerequisite


**Scope Based on Decisions**:
- ❌ Skip carryover args accessors (eliminated in sentence-args refactor prerequisite)
- ✅ Include OLD_ED accessors (`ed_buffer`, net_dead check)
- ✅ Include efun accessors (local_port, peer_ip, peer_port)
- ❌ Skip F_TRACE accessors (feature disabled)

**Implementation**:
- See full accessor list in "Required Accessors" section
- ~20-25 accessor functions for interactive_t state (reduced from 25-30)
- Includes OLD_ED support (ed_buffer accessors)
- No carryover or F_TRACE accessors needed

### Integration Tests

1. **Manual testing script** (testbot.py):
   ```python
   # Test input_to callbacks (already extensively tested in prerequisite)
   send("input_to_echo_test\\n")
   expect("Enter text:")
   send("hello world\\n")
   expect("You entered: hello world")
   
   # Test add_action with carryover args (new capability from prerequisite)
   send("test_add_action_args\\n")
   expect("Command executed with context")
   
   # Test get_char single character mode
   send("get_char_test\\n")
   expect("Press any key:")
   send("x")  # Single character
   expect("You pressed: x")
   ```

2. **Mudlib integration**:
   - Load existing M3 mudlib
   - Test login sequence (uses input_to for password)
   - Test editor integration (if OLD_ED enabled)
   - Test command processing under load

## Risk Assessment

### High Risk
- **Performance regression** - Accessor overhead
  - Mitigation: Inline functions, measure benchmarks
  
- **Build breakage** - Circular dependencies, missing includes
  - Mitigation: Phased approach, frequent builds

- **OLD_ED integration** - ed_buffer accessor testing
  - Mitigation: Test with OLD_ED enabled, verify ed integration

### Low Risk
- **Behavioral changes** - Command processing semantics
  - Mitigation: Move code as-is, refactor later

## Success Criteria

1. ✅ **Modularity**: comm.c ~1800 lines, user_command.c ~1200 lines, simulate.c reduced by ~150 lines
2. ✅ **Encapsulation**: `interactive_t` opaque to user_command.c
3. ✅ **Safety**: All protocol callbacks have error handling
4. ✅ **Functionality**: `input_to()` and `get_char()` work correctly in new location (already refactored in prerequisite)
5. ✅ **add_action() carryover**: New varargs capability works (from prerequisite)
6. ✅ **Testing**: Full test coverage, no regressions
7. ✅ **Performance**: <5% overhead on command processing
8. ✅ **OLD_ED Support**: ed_buffer accessors work correctly
9. ✅ **Documentation**: Update internals.md with new architecture

## Timeline Estimate

- **Phase 1**: 2-3 days (module boundary, input_to/get_char move)
- **Phase 2**: 1-2 days (error handling)
- **Phase 3**: 2-3 days (backend integration)
- **Phase 4**: 3-4 days (buffer API)
- **Phase 5**: 2-3 days (protocol mode API)
- **Phase 6**: 3-4 days (accessors, no carryover accessors needed)
- **Phase 7**: 1-2 days (opaque type)

**This Refactor**: ~15-20 working days (~3-4 weeks)

**Total with Prerequisites**: ~24-31 working days (~5-6 weeks)

**Adjustment rationale**:
- Phase 1: Reduced complexity (input_to/get_char already refactored)
- Phase 6: -1 day (no carryover accessors needed)
- Prerequisites add initial ~2 weeks but reduce overall risk

### Regression Testing

- Run full test suite after each phase
- Check with Valgrind/ASan for memory issues
- Monitor performance (command processing latency)
- Test on all platforms (Windows, Linux, macOS)

## Migration Notes

### For Mudlib Developers

**Breaking changes for efuns using implicit command_giver**:
- `set_prompt(string)` → Driver now requires explicit object parameter internally
- `set_notify_fail(mixed)` → Driver now requires explicit object parameter internally
- **Efun signatures unchanged** - Wrapper functions use `command_giver` to call new API
- `input_to(function, flags, ...)` - No change (already moved to user_command.c)
- `query_ip_name(object)` - No change
- `set_snoop(snoopee, snooper)` - No change

**New apply safety**: Terminal callbacks now safer:
```lpc
void terminal_type(string type) {
    // Can now safely call destruct(this_object())
    // Driver handles gracefully
}
```

### For Driver Developers

**New Files**:
- `src/user_command.c` - Command dispatch layer
- `src/user_command.h` - Public API for command processing

**Modified Files**:
- `src/comm.c` - Stream layer only
- `src/comm.h` - Stream API + opaque interactive_t
- `src/backend.c` - Uses new comm API functions

**Removed Globals**:
- `all_users[]` - Now private to comm.c
  - Use `lookup_user_at(int index)` for array access
  - Use per-object functions for user operations

**New API Functions**: See accessor list in "Required Accessors" section

**Changed API Signatures** (implicit → explicit):
- `set_prompt(char *)` → `set_prompt(object_t *, const char *)`
- `set_notify_fail_message(char *)` → `set_notify_fail_message(object_t *, const char *)`
- `set_notify_fail_function(funptr_t *)` → `set_notify_fail_function(object_t *, funptr_t *)`

**Note**: Efun implementations will adapt by passing `command_giver` to new signatures

## Risk Assessment

### High Risk
- **Protocol callback error handling** - Mudlib code can destruct objects
  - Mitigation: VALIDATE_IP checks, extensive testing
  
- **Backend integration** - Turn management is critical for fairness
  - Mitigation: Preserve exact semantics, add tests

### Medium Risk
- **Performance regression** - Accessor overhead
  - Mitigation: Inline functions, measure benchmarks
  
- **Build breakage** - Circular dependencies, missing includes
  - Mitigation: Phased approach, frequent builds

### Low Risk
- **Behavioral changes** - Command processing semantics
  - Mitigation: Move code as-is, refactor later

## Known Technical Debt

### Apply Cache Flags on interactive_t

**Issue**: `HAS_PROCESS_INPUT` and `HAS_WRITE_PROMPT` flags are stored on `interactive_t->iflags` but semantically describe object program attributes (whether the LPC code defines these applies).

**Why It's Wrong**:
1. Object attributes should be on `object_t->flags` (like `O_HEART_BEAT`, `O_WILL_CLEAN_UP`)
2. Information is lost if object loses/regains interactive status
3. Inconsistent with other capability flags

**Why Not Fixed Now**:
- This refactor already touches significant architecture
- Fixing requires adding fields to `object_t` or implementing program-level caching
- Needs program change invalidation logic (currently non-existent)

**Mitigation**:
- Accessor names emphasize caching behavior: `has_process_input_apply()`, `cache_process_input_apply()`
- Names work regardless of future storage location
- See [docs/plan/object-apply-flags-analysis.md](object-apply-flags-analysis.md) for full analysis

**Future Work**: Move to `object_t->flags` with program change invalidation in a separate refactor.

## Success Criteria

1. ✅ **Modularity**: comm.c ~1800 lines, user_command.c ~1200 lines
2. ✅ **Encapsulation**: `interactive_t` opaque to user_command.c
3. ✅ **Safety**: All protocol callbacks have error handling
4. ✅ **Testing**: Full test coverage, no regressions
5. ✅ **Performance**: <5% overhead on command processing
6. ✅ **Documentation**: Update internals.md with new architecture

## Timeline Estimate

- **Phase 1**: 2-3 days (module boundary)
- **Phase 2**: 1-2 days (error handling)
- **Phase 3**: 2-3 days (backend integration)
- **Phase 4**: 3-4 days (buffer API)
- **Phase 5**: 2-3 days (protocol mode API)
- **Phase 6**: 4-5 days (accessors)
- **Phase 7**: 1-2 days (opaque type)

**Total**: ~15-22 working days (~3-4 weeks)

## Future Work

After separation complete:

1. **Three-layer architecture**: 
   - Stream I/O (comm.c)
   - Protocol (telnet_protocol.c, console_protocol.c)
   - Command dispatch (user_command.c)

2. **Async I/O improvements**:
   - Buffered writes with completion notifications
   - Zero-copy send for large messages

3. **Protocol callbacks as function pointers**:
   - Invert dependency (comm.c doesn't call apply)
   - Register callbacks from user_command.c

4. **Alternative protocols**:
   - WebSocket support
   - SSH support
   - All isolated to protocol layer

## References

- [Development Guide](docs/manual/dev.md) - Modularity section
- [Async Library Design](docs/internals/async-library.md)
- Original analysis: See conversation 2026-02-06
