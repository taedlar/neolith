# User Command Turn-Based Fairness System

**Status**: ✅ Implemented and tested  
**Version**: neolith-1.0.0-alpha  
**Date**: January 2026

## Summary

This document describes the implementation of a turn-based fairness system that prevents individual users from monopolizing backend processing cycles through command buffering. The solution ensures each connected user processes exactly one buffered command per backend cycle, providing fair round-robin scheduling across all users.

## Problem Statement

### Previous Behavior (Before Fix)

The backend loop previously processed user commands with this pattern:

```c
for (i = 0; process_user_command() && i < max_users; i++);
```

This limited the number of **calls** to `process_user_command()` but not the number of commands **per user**. A single user with multiple buffered commands could monopolize the entire backend cycle.

### Vulnerability

**Scenario**: With `max_users = 10`:
- **User A**: 10 buffered commands
- **Slots 0, 2-9**: NULL or users without commands

**Exploitation**:
1. `get_user_command()` used a static counter `s_next_user` (formerly `NextCmdGiver` in MudOS) to iterate round-robin
2. Empty slots caused counter to advance quickly, cycling back to active users
3. User A could be selected multiple times in one cycle (up to 10 times)
4. **Result**: User A processed all 10 commands while other users starved

### Root Cause

The static counter `s_next_user` advanced independently of whether a command was found, and the loop limit counted **invocations** not **distinct users**. Combined with:
- Empty slots in `all_users[]` (disconnected users)
- `max_users` only growing, never shrinking
- Users with deep command queues

This allowed one user to consume all processing slots in a single backend cycle, creating an unfair advantage in time-sensitive situations (combat, resource gathering, etc.).

## Solution Implemented

### Turn-Based Token System

A **turn token system** ensures each user receives exactly one "turn" per backend cycle:

1. **Grant turns**: At cycle start, set `HAS_CMD_TURN` flag for all connected users
2. **Check turn**: `get_user_command()` only returns commands for users with available turns
3. **Consume turn**: Clear `HAS_CMD_TURN` when command is processed
4. **Natural termination**: Loop stops when all turns exhausted or no users have commands

### Key Improvements

**Fairness**:
- Each user processes exactly **1 command per backend cycle**
- Round-robin scheduling prevents any user from monopolizing processing
- Users with deep command queues don't gain unfair advantage

**Performance**:
- Loop bounded by `connected_users` (actual count) instead of `max_users` (historical peak)
- Example: 2 connected users → 2 iteration max, not 100
- Zero timeout when unprocessed commands remain (improves responsiveness)

**Correctness**:
- `command()` efun bypasses turn system (LPC-initiated commands unaffected)
- Game logic (`heart_beat()`, `call_out()`, NPC actions) continues normally
- Turn limitation targets only network input buffering exploit

### Technical Details

**Flag Definition** ([comm.h](../../src/comm.h)):
```c
#define HAS_CMD_TURN    0x1000   /* User has command processing turn this cycle */
```

**Scope**: Turn limitation applies **only to user input commands** buffered from network connections.

**Unaffected operations**:
- `command()` efun (calls `process_command()` directly, bypassing `get_user_command()`)
- `call_out()` callbacks (timer-based execution)
- `heart_beat()` functions (periodic object updates)
- NPC actions (use `command()` efun)
- Mudlib aliases/macros (use `command()` efun)

**Rationale**: LPC-initiated commands are already rate-limited by `eval_cost` and don't consume network I/O resources. The turn system specifically addresses the network input buffering exploit.

## Implementation Details

### 1. Backend Loop Modifications

**File**: [src/backend.c](../../src/backend.c)

The backend loop now performs three operations in a single efficient pass:

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

### 2. Command Selection Modifications

**File**: [src/comm.c](../../src/comm.c), function `get_user_command()`

The command selection logic now checks for turn availability:

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

When a user with commands but no turn is encountered, the loop continues searching for other users who have both commands and turns.

## Execution Flow

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

### Example Scenario

**Setup**: 2 connected users, `max_users = 10`
- **User A** (slot 5): 10 buffered commands
- **User B** (slot 7): 1 buffered command

**Cycle 1**: Grant turns to both users (`connected_users = 2`)
- Process User A command 1 (turn consumed)
- Process User B command 1 (turn consumed)
- User A has 9 remaining but no turn → skip
- Loop terminates (no users with turns+commands)

**Cycle 2**: Grant turns to both users
- Process User A command 2 (turn consumed)
- User B has no commands → skip
- Loop terminates

**Result**: Fair processing with loop bounded at 2 iterations (not 10).

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

**File**: [test_command_fairness.cpp](../../tests/test_backend/test_command_fairness.cpp)

Tests cover:
- Basic fairness (2 users with multiple commands each)
- Queue depth variations (asymmetric command counts)
- Empty slot handling (sparse `all_users[]` array)
- Disconnection during processing
- Single character mode compatibility
- Timeout calculation with pending commands
- Flag manipulation and turn consumption

All 11 tests passing.

## Performance Considerations

- **Turn grant overhead**: O(max_users) per cycle, negligible cost (NULL check + bit set + counter)
- **Pending command check**: Integrated into turn grant loop, stops at first match
- **Loop iteration**: Bounded by `connected_users` not `max_users` (tighter safety limit)
- **Normal operation**: Loop typically terminates when all turns exhausted, bound rarely reached

## Migration Notes

### Compatibility

- No LPC API changes
- No mudlib modifications required
- Transparent to game developers
- `command()` efun behavior unchanged (immediate execution, no turn restrictions)

### Configuration

No new configuration options needed.

### Behavioral Changes

**User input**: Limited to 1 buffered command per user per backend cycle  
**Timeout**: Zero when unprocessed commands remain (improves responsiveness)  
**Loop bound**: Uses `connected_users` instead of `max_users`

**Unchanged**: `command()` efun, `call_out()`, `heart_beat()`, and all LPC-initiated commands

### Implementation Improvements

**Optimization**: Loop bound uses `connected_users` (actual count) instead of `max_users` (historical peak).

**Benefits**: Reduces wasted iterations, improves failure containment, more accurate loop semantics.

## Future Enhancements

- **Priority levels**: Different turn quotas for interactive users vs automated processes
- **Adaptive timeout**: Dynamic timeout based on command queue depth (0ms for deep queues, 1ms for shallow, 60s for empty)

## References

- [backend.c](../../src/backend.c) - Main backend loop implementation
- [comm.c](../../src/comm.c) - User command processing and turn checking
- [comm.h](../../src/comm.h) - Interactive flags including HAS_CMD_TURN
- [test_command_fairness.cpp](../../tests/test_backend/test_command_fairness.cpp) - Unit tests
- [ChangeLog.md](../ChangeLog.md) - Release notes

## Implementation Status

### Completed ✅

- Added `HAS_CMD_TURN` flag ([comm.h](../../src/comm.h))
- Implemented combined turn grant/counting/pending check loop ([backend.c](../../src/backend.c))
- Modified timeout logic for pending commands ([backend.c](../../src/backend.c))
- Updated command loop to use `connected_users` bound ([backend.c](../../src/backend.c))
- Modified `get_user_command()` to check/consume turns ([comm.c](../../src/comm.c))
- Created 11 unit tests - all passing ([test_command_fairness.cpp](../../tests/test_backend/test_command_fairness.cpp))
- Updated [ChangeLog.md](../ChangeLog.md)

### Optional Future Work

- Integration tests in LPC mudlib
- Manual testing with telnet
- Performance profiling (100+ users)

## Validation

✅ **Unit tests**: All 11 tests passing  
✅ **Code review**: Implementation matches design  
✅ **Static analysis**: No warnings/errors  
✅ **Design verification**: `command()` efun bypasses turn system

**Status**: Ready for merge
