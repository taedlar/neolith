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

## Implementation Details

### 1. Backend Loop Modifications

**File**: `src/backend.c`

**Before command processing** (add after line 309):

```c
/* Grant command processing turn to all connected users */
for (i = 0; i < max_users; i++)
{
    if (all_users[i])
    {
        all_users[i]->iflags |= HAS_CMD_TURN;
    }
}
```

**Timeout calculation** (modify around line 285):

```c
/* Check if any user has unprocessed commands */
int has_pending_commands = 0;
for (i = 0; i < max_users; i++)
{
    if (all_users[i] && (all_users[i]->iflags & CMD_IN_BUF))
    {
        has_pending_commands = 1;
        break;
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

### Scenario
- **max_users**: 10
- **User A** (slot 5): 10 buffered commands
- **User B** (slot 7): 1 buffered command
- **Other slots**: NULL or no commands

### Backend Cycle 1

1. **Grant turns**: `all_users[5]->iflags |= HAS_CMD_TURN` and `all_users[7]->iflags |= HAS_CMD_TURN`

2. **Iteration 1**:
   - `s_next_user = 5` (hypothetically)
   - Find User A: has command + has turn → consume turn, process command 1

3. **Iteration 2**:
   - Search from `s_next_user = 4, 3, 2...`
   - Find User B at slot 7: has command + has turn → consume turn, process command 1

4. **Iteration 3**:
   - Search all users
   - User A has 9 commands but **no turn** → skip
   - No other users with commands+turns → return 0, stop loop

### Backend Cycle 2

1. **Grant turns**: Both users get turns again

2. **Iteration 1**: Process User A's command 2

3. **Iteration 2**: User B has no more commands, skip

4. **Iteration 3**: User A has commands but no turn → stop

**Result**: Fair processing - each user gets 1 command per cycle regardless of queue depth.

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

### Empty Slots Performance

- Turn grant loops through all `max_users` slots (including NULLs)
- With `max_users` never shrinking, could iterate 100+ slots if peak was high
- Performance impact: negligible (simple pointer check + bit operation)
- Alternative: track `num_user` for early break (optimization for later)

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

### Manual Testing

1. Connect multiple telnet sessions
2. Paste long command sequences into one session
3. Type commands normally in other sessions
4. Observe fair command processing via debug logging

## Performance Considerations

### Turn Grant Overhead

- **Cost**: O(max_users) iteration per backend cycle
- **Operations**: NULL check + bit set operation
- **Impact**: Negligible (microseconds for 100 users)

### Pending Command Check

- **Cost**: O(max_users) iteration per backend cycle
- **Early exit**: Breaks on first match
- **Alternative**: Set global flag in `get_user_command()` when skipping due to no turn
- **Decision**: Use scan approach for simplicity and correctness

### Loop Iteration Reduction

- **Before**: Up to `max_users` calls to `process_user_command()`
- **After**: Typically `num_user` calls (actual connected users)
- **Benefit**: Fewer wasted iterations searching empty slots

## Migration Notes

### Compatibility

- No changes to LPC API
- No mudlib changes required
- Transparent to game developers
- Only affects internal scheduling

### Configuration

No new configuration options needed. The fairness is enforced automatically.

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

### max_users Compaction

Currently `max_users` only grows. Could implement periodic compaction:
- When `num_user` << `max_users`, compact the array
- Requires updating all index references
- Trade-off: complexity vs. minor performance gain

## References

- [backend.c](../../src/backend.c) - Main backend loop
- [comm.c](../../src/comm.c) - User command processing
- [comm.h](../../src/comm.h) - Interactive flags and structures
- [docs/manual/internals.md](../manual/internals.md) - Driver architecture

## Implementation Checklist

1. ✓ Design document created
2. [ ] Add `HAS_CMD_TURN` flag to comm.h
3. [ ] Implement turn grant logic in backend.c
4. [ ] Implement timeout check logic in backend.c
5. [ ] Modify get_user_command() to check/consume turns
6. [ ] Add unit tests for fairness scenarios
7. [ ] Add integration tests with test mudlib
8. [ ] Manual testing with multiple connections
9. [ ] Performance profiling with high user count
10. [ ] Update ChangeLog.md
