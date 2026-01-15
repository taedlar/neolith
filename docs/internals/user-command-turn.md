# User Command Turn-Based Fairness System

## Problem Statement

### Current Behavior

The backend loop processes user commands with this pattern:

```c
for (i = 0; process_user_command() && i < max_users; i++);
```

This limits the number of **calls** to `process_user_command()` but not the number of commands per user. A single user with multiple buffered commands can monopolize the entire backend cycle.

### Exploit Scenario

With `max_users = 10`:
- **User A**: 10 buffered commands
- **Slots 0, 2-9**: NULL or users without commands

**What happens:**
1. `get_user_command()` uses static counter `s_next_user` to iterate round-robin
2. Empty slots cause counter to advance quickly
3. User A gets selected multiple times in one cycle
4. **Result**: User A processes up to 10 commands while other users starve

### Root Cause

The static counter `s_next_user` advances independently of whether a command was found, and the loop limit counts **invocations** not **distinct users**. Combined with:
- Empty slots in `all_users[]` (disconnected users)
- `max_users` only grows, never shrinks
- Users with deep command queues

This allows one user to consume all processing slots in a single backend cycle.

## Design Solution

### Core Concept: Turn-Based Processing

Implement a **turn token system** where each user receives exactly one "turn" per backend cycle:

1. **Grant turns**: At cycle start, set `HAS_CMD_TURN` flag for all connected users
2. **Check turn**: `get_user_command()` only returns commands for users with turns
3. **Consume turn**: Clear `HAS_CMD_TURN` when command is processed
4. **Natural termination**: Loop stops when all turns exhausted or no users have commands

### Flag Definition

```c
#define HAS_CMD_TURN    0x1000   /* User has command processing turn this cycle */
```

Added to `interactive_t->iflags` in [comm.h](../../src/comm.h).

### Scope of Turn Limitation

**Important**: The turn limitation applies **only to user input commands** buffered from network connections. It does **NOT** affect:

- **`command()` efun**: LPC code calling `command("look", player)` bypasses turn checking entirely
- **`call_out()` callbacks**: Delayed function execution is unaffected
- **`heart_beat()` functions**: Periodic object updates are unaffected
- **NPC actions**: Commands initiated by game objects use `command()` efun
- **Aliases/macros**: Mudlib command preprocessing via `command()` efun

This is correct behavior because:
- LPC-initiated commands are already rate-limited by `eval_cost`
- They don't consume network I/O resources (no buffering)
- They're part of game logic, not player input queuing
- Turn limitation targets network input buffering exploit only

## Implementation Details

### 1. Backend Loop Modifications

**File**: `src/backend.c`

**Combined turn grant, user counting, and timeout calculation** (modify around line 285):

```c
/* Check if any user has unprocessed commands.
 * Combined with turn grant loop for efficiency.
 */
int has_pending_commands = 0;
int connected_users = 0;
for (i = 0; i < max_users; i++)
{
    if (all_users[i])
    {
        all_users[i]->iflags |= HAS_CMD_TURN;
        connected_users++;
        
        if (!has_pending_commands && (all_users[i]->iflags & CMD_IN_BUF))
        {
            has_pending_commands = 1;
        }
    }
}

if (heart_beat_flag || has_pending_commands)
{
    /* Don't wait in poll - process immediately */
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
}
else
{
    /* Can wait for new I/O events */
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;
}

/* Process user commands fairly (round-robin).
 * Limit to connected_users for tighter safety bound.
 */
for (i = 0; process_user_command() && i < connected_users; i++);
```

### 2. Command Processing Modifications

**File**: `src/comm.c`, function `get_user_command()`

**Current logic** (around line 1903):

```c
if (ip && ip->iflags & CMD_IN_BUF)
{
    user_command = first_cmd_in_buf(ip);
    if (user_command)
        break;
    else
        ip->iflags &= ~CMD_IN_BUF;
}
```

**Modified logic**:

```c
if (ip && ip->iflags & CMD_IN_BUF)
{
    user_command = first_cmd_in_buf(ip);
    if (user_command)
    {
        /* Check if user has their turn */
        if (ip->iflags & HAS_CMD_TURN)
        {
            ip->iflags &= ~HAS_CMD_TURN;  /* Consume turn */
            break;  /* Process this command */
        }
        else
        {
            /* User has command but no turn - skip and continue searching */
            user_command = NULL;
        }
    }
    else
    {
        ip->iflags &= ~CMD_IN_BUF;
    }
}
```

**Loop termination**:

After the loop (around line 1916), the existing check works correctly:

```c
if (!ip || !user_command)
    return 0;  /* No users with turns+commands found */
```

### 3. Header File Modifications

**File**: `src/comm.h`

Add flag definition after line 37 (after `USING_LINEMODE`):

```c
#define	USING_LINEMODE      0x0800
#define HAS_CMD_TURN        0x1000   /* User has command processing turn this cycle */
```

## Execution Flow Example

### User Input Command Flow (Turn-Limited)

**Path**: Network → Buffer → Backend Loop → Turn Check → Execution

```
User types command
  → get_user_data() reads from socket
    → copy_chars() processes telnet, adds to interactive_t->text buffer
    → Sets CMD_IN_BUF flag
    
Backend Loop
  → Grant HAS_CMD_TURN to all connected users
  → process_user_command()
    → get_user_command() [CHECKS HAS_CMD_TURN]
      → first_cmd_in_buf() extracts command from buffer
      → IF has turn: Clear HAS_CMD_TURN [CONSUME TURN]
      → ELSE: Skip user, continue searching
      → Return command string
    → process_command(command, user)
      → user_parser() executes LPC actions
```

### command() Efun Flow (NOT Turn-Limited)

**Path**: LPC Code → Direct Execution (bypasses turn system)

```
LPC code: command("look", player)
  → f_command() in lib/efuns/command.c
    → command_for_object()
      → process_command() [DIRECTLY, NO TURN CHECK]
        → user_parser() executes LPC actions
```

The `command()` efun calls `process_command()` directly without going through `get_user_command()`, so turn checking never occurs.

### Scenario
- **max_users**: 10
- **User A** (slot 5): 10 buffered commands
- **User B** (slot 7): 1 buffered command
- **Other slots**: NULL or no commands

### Backend Cycle 1

1. **Grant turns and count users**: 
   - `all_users[5]->iflags |= HAS_CMD_TURN` and `all_users[7]->iflags |= HAS_CMD_TURN`
   - `connected_users = 2`
   - Loop limit set to 2 (not 10)

2. **Iteration 1** (`i=0`):
   - `s_next_user = 5` (hypothetically)
   - Find User A: has command + has turn → consume turn, process command 1

3. **Iteration 2** (`i=1`):
   - Search from `s_next_user = 4, 3, 2...`
   - Find User B at slot 7: has command + has turn → consume turn, process command 1

4. **Iteration 3** (`i=2`, at loop limit):
   - Would search all users
   - User A has 9 commands but **no turn** → skip
   - No other users with commands+turns → `process_user_command()` returns 0
   - Loop terminates (both conditions met: return 0 AND `i < 2` is now false)

### Backend Cycle 2

1. **Grant turns and count**: Both users get turns, `connected_users = 2`

2. **Iteration 1** (`i=0`): Process User A's command 2

3. **Iteration 2** (`i=1`): 
   - User B has no more commands (buffer empty)
   - Continue searching → no other users with commands+turns
   - `process_user_command()` returns 0, loop terminates

**Result**: Fair processing - each user gets 1 command per cycle regardless of queue depth.

**Loop Efficiency**: With only 2 connected users, loop bounded at 2 iterations instead of 10 (the `max_users` value).

## Edge Cases

### User Disconnects Mid-Cycle

- **Turn grant loop**: `all_users[i]` becomes NULL → safely skipped with null check
- **Command processing**: Existing disconnection handling works unchanged

### User Connects Mid-Cycle

- **New user**: Won't have `HAS_CMD_TURN` flag set
- **Behavior**: Waits until next cycle for turn grant (fair)

### Partial Commands

- `first_cmd_in_buf()` returns NULL → user skipped
- `CMD_IN_BUF` flag cleared
- Turn preserved (not consumed)

### Single Character Input Mode

- `cmd_in_buf()` returns 1 for any buffered data in `SINGLE_CHAR` mode
- Turn consumed per character processed
- Zero timeout when `CMD_IN_BUF` set ensures responsive processing

### command() Efun Usage

**Scenario**: User A has exhausted their turn, but their `heart_beat()` calls `command("emote laughs")`

- User A's buffered input commands: **Blocked** (no turn remaining)
- `command()` efun execution: **Proceeds normally** (bypasses turn system)
- Result: NPC actions and mudlib automation continue regardless of turn status

This allows game logic to function independently of player input queuing.

## Testing Strategy

### Unit Tests

**Test file**: `tests/test_backend/test_command_fairness.cpp`

1. **Basic fairness**: 2 users with multiple commands each
   - Verify each processes exactly 1 command per cycle
   
2. **Queue depth variations**: User A has 10 commands, User B has 1
   - Verify both process 1 per cycle until B exhausted
   
3. **Empty slots**: Sparse `all_users[]` with NULLs
   - Verify fairness not affected by empty slots
   
4. **Disconnection**: User disconnects with pending commands
   - Verify no crashes, other users unaffected
   
5. **Single character mode**: User in `SINGLE_CHAR` mode with buffered input
   - Verify turn consumed per character
   
6. **Timeout calculation**: Users with pending commands
   - Verify timeout is zero when commands remain after turn exhaustion

### Integration Tests

**Test mudlib**: `examples/m3_mudlib/`

1. **Spam test**: Multiple users sending rapid command sequences
   - Verify even command processing distribution
   
2. **Combat simulation**: Time-sensitive command processing
   - Verify no user gains unfair advantage through command buffering
   
3. **Latency test**: Measure command response time with varying queue depths
   - Verify predictable latency regardless of other users' queues

4. **command() efun test**: Create NPC with `heart_beat()` that uses `command()`
   - Verify NPC actions work regardless of player turn status
   - Verify `command()` doesn't consume player turns

### Manual Testing

1. Connect multiple telnet sessions
2. Paste long command sequences into one session
3. Type commands normally in other sessions
4. Observe fair command processing via debug logging
5. Test `command()` efun from LPC code while user has buffered commands

## Performance Considerations

### Turn Grant Overhead

- **Cost**: O(max_users) iteration per backend cycle
- **Operations**: NULL check + bit set operation + counter increment
- **Impact**: Negligible (microseconds for 100 users)
- **Benefit**: Produces `connected_users` count used to tighten command loop bound

### Pending Command Check

- **Cost**: O(max_users) iteration per backend cycle, combined with turn grant
- **Optimization**: Stops checking once first pending command found
- **No additional overhead**: Integrated into existing turn grant loop

### Loop Iteration Reduction

- **Before**: Loop bounded at `max_users` (historical peak, e.g., 100)
- **After**: Loop bounded at `connected_users` (actual count, e.g., 2)
- **Benefit**: Tighter safety limit, fewer wasted iterations in edge cases
- **Normal termination**: Loop typically stops when `process_user_command()` returns 0 (all turns exhausted)
- **Safety bound**: `connected_users` limit prevents excessive iteration if bugs occur
- **Example**: If `max_users=100` but only 2 users connected, loop limit is 2 not 100

## Migration Notes

### Compatibility

- No changes to LPC API
- No mudlib changes required
- Transparent to game developers
- Only affects internal scheduling
- **`command()` efun behavior unchanged**: Continues to execute immediately without turn restrictions

### Configuration

No new configuration options needed. The fairness is enforced automatically.

### Behavioral Changes

**What changes**:
- User input commands buffered from network are limited to 1 per backend cycle per user
- Zero timeout when buffered commands remain unprocessed (improves responsiveness)
- Command processing loop uses tighter bound (`connected_users` instead of `max_users`)

**What doesn't change**:
- `command()` efun execution (immediate, no turn checking)
- `call_out()` execution (timer-based, not turn-limited)
- `heart_beat()` execution (periodic, not turn-limited)
- Any LPC-initiated command processing
- Overall command throughput (same commands processed, just distributed fairly)

### Implementation Improvements

This design includes an optimization over the naive approach:

**Naive approach**: 
```c
for (i = 0; process_user_command() && i < max_users; i++);
```
Loop bounded by historical peak user count.

**Optimized approach**:
```c
int connected_users = 0;
for (i = 0; i < max_users; i++) {
    if (all_users[i]) {
        all_users[i]->iflags |= HAS_CMD_TURN;
        connected_users++;
    }
}
for (i = 0; process_user_command() && i < connected_users; i++);
```

**Benefits**:
- Tighter safety bound (actual users, not historical peak)
- More accurate semantics (loop limit matches connected users)
- Better failure mode containment (bug protection)
- Minimal cost (single counter increment per user)
- Self-contained (no dependency on global `num_user` accuracy)

## Future Enhancements

### Priority Levels

Could extend to support priority-based scheduling:
- Interactive users: 1 turn per cycle
- Automated processes: 1 turn per N cycles
- Requires additional flags and turn counter

### Adaptive Timeout

Could adjust timeout based on command queue depth:
- Deep queues: timeout = 0
- Shallow queues: timeout = 1ms (allow brief I/O wait)
- Empty queues: timeout = 60s

## References

- [backend.c](../../src/backend.c) - Main backend loop
- [comm.c](../../src/comm.c) - User command processing
- [comm.h](../../src/comm.h) - Interactive flags and structures
- [docs/manual/internals.md](../manual/internals.md) - Driver architecture

## Implementation Checklist

1. ✓ Design document created
2. ✓ Add `HAS_CMD_TURN` flag to comm.h
3. ✓ Implement turn grant logic in backend.c
4. ✓ Implement timeout check logic in backend.c
5. ✓ Modify get_user_command() to check/consume turns
6. ✓ Add unit tests for fairness scenarios
7. ✓ Update ChangeLog.md
