# Console Testbot Phase 2 - Windows Piped Stdin Support

**Date**: January 16, 2026  
**Status**: ✅ Complete  
**Phase**: 2 of 3  

## Summary

Implemented Windows piped stdin support for console mode, enabling `testbot.py` to work cross-platform. The implementation uses synchronous `ReadFile()` for pipes/files, avoiding unnecessary complexity while maintaining the same behavioral pattern as Phase 1's POSIX approach.

## Components Delivered

### 1. Handle Type Detection

**File**: [lib/port/io_reactor.h](../../lib/port/io_reactor.h)

Added `console_type_t` enum to distinguish input handle types:
```c
typedef enum {
    CONSOLE_TYPE_NONE = 0,
    CONSOLE_TYPE_REAL,    /* Real console (ReadConsoleInputW) */
    CONSOLE_TYPE_PIPE,    /* Pipe (synchronous ReadFile) */
    CONSOLE_TYPE_FILE     /* File (synchronous ReadFile) */
} console_type_t;
```

### 2. Reactor Handle Detection

**File**: [lib/port/io_reactor_win32.c](../../lib/port/io_reactor_win32.c)

Modified `io_reactor_add_console()` to detect handle type:
- Uses `GetFileType()` to determine `FILE_TYPE_CHAR`, `FILE_TYPE_PIPE`, or `FILE_TYPE_DISK`
- Real consoles validated with `GetConsoleMode()`
- Pipes and files use simplified synchronous read approach

### 3. Simplified I/O for Pipes

**File**: [src/comm.c](../../src/comm.c)

Updated `get_user_data()` to use synchronous `ReadFile()` for pipes/files:
- No overlapped I/O complexity
- Same pattern as POSIX `read()` - just block and read
- Mirrors Phase 1's philosophy: keep it simple

### 4. EOF Handling

**File**: [src/comm.c](../../src/comm.c)

Modified `remove_interactive()` to detect pipe EOF:
- Windows: Uses `io_reactor_get_console_type()` to check for `CONSOLE_TYPE_PIPE` or `CONSOLE_TYPE_FILE`
- POSIX: Uses `isatty()` to check if stdin is not a TTY
- Pipes/files: Call `do_shutdown(0)` for clean exit
- Real consoles: Display reconnection prompt (existing behavior)

### 5. IOCP Reactor Fixes

**File**: [lib/port/io_reactor_win32.c](../../lib/port/io_reactor_win32.c)

Fixed fundamental IOCP architecture issues discovered during testing:

**Problem**: IOCP completion handles cannot be waited on with `WaitForMultipleObjects()`, causing socket I/O events to timeout even when data was available.

**Solution**: 
- When console/listening sockets exist: Wait on those first, then check IOCP non-blocking
- When only wakeup event exists: Use short timeout on wakeup, then wait on IOCP with remaining time
- Ensures IOCP completions are checked with appropriate timeout

### 6. Wakeup Mechanism Fix

**File**: [lib/port/io_reactor_win32.c](../../lib/port/io_reactor_win32.c)

Fixed `io_reactor_wakeup()` to signal both event handle AND IOCP:
- Uses `PostQueuedCompletionStatus(iocp_handle, 0, 0, NULL)` to interrupt IOCP waits
- Detects wakeup completions by checking for `overlapped == NULL`
- Ensures timer interrupts work regardless of which wait mechanism is active

### 7. Mudlib Enhancements

**File**: [examples/m3_mudlib/user.c](../../examples/m3_mudlib/user.c)

Added `shutdown` command:
- Calls `shutdown()` efun to cleanly exit the driver
- Useful for interactive console testing
- Provides explicit shutdown method beyond pipe EOF

**File**: [examples/testbot.py](../../examples/testbot.py)

Updated test automation:
- Uses `shutdown` command in test sequence
- Reduced timeout from 10s to 5s (pipe EOF now exits cleanly)
- Updated documentation to reflect cross-platform support

## Test Results

### Unit Tests
- ✅ **37/37 io_reactor tests passing** (was 29/37)
- ✅ All socket I/O tests work correctly
- ✅ All wakeup tests work correctly  
- ✅ Console mode tests pass

### Integration Tests
- ✅ `testbot.py` passes on Windows with exit code 0
- ✅ Piped stdin processed correctly until EOF
- ✅ Driver shuts down cleanly on pipe closure
- ✅ Real console mode still works interactively

### Manual Testing
```powershell
# Pipe test
"say test`nhelp`nshutdown" | .\neolith.exe -f m3.conf -c
# ✅ All commands processed, clean exit

# Python testbot
cd examples; python testbot.py
# ✅ TEST PASSED - Driver exited successfully
```

## Key Design Decisions

### 1. Synchronous ReadFile() Instead of Overlapped I/O

**Initial Approach**: Attempted to use overlapped I/O with manual event signaling for pipes.

**Problems**:
- Complex implementation with overlapped structures and events
- IOCP handles can't be waited on with `WaitForMultipleObjects()`
- `GetOverlappedResult()` failing or not signaling properly
- PowerShell pipes closing immediately caused timing issues

**Final Solution**: Use synchronous `ReadFile()` - simple and works.

**Rationale**: Phase 1 succeeded because it was simple (just avoid flushing with `isatty()`). Phase 2 should mirror that philosophy. Synchronous reads are perfectly fine for console stdin - it's not a performance-critical path like network sockets.

### 2. EOF Triggers Shutdown for Pipes

**Behavior**: When pipe/file stdin reaches EOF, immediately call `do_shutdown(0)` instead of attempting console reconnection.

**Rationale**: 
- Pipes can't be "reconnected" like real consoles
- Automated tests need clean exit
- Distinguishes testing automation from interactive use

### 3. Removed Unused Code

Cleaned up after simplification:
- Removed `iocp_context_t` export from public header (kept as internal definition)
- Removed unused `console_event` and `console_ctx` fields
- Removed accessor functions that became unnecessary
- Kept only `console_type` field and `io_reactor_get_console_type()` accessor

## Files Modified

### Core Implementation
- [lib/port/io_reactor.h](../../lib/port/io_reactor.h) - Added console_type_t enum
- [lib/port/io_reactor_win32.c](../../lib/port/io_reactor_win32.c) - Handle detection, IOCP fixes, wakeup fix
- [src/comm.c](../../src/comm.c) - Synchronous ReadFile() for pipes, EOF shutdown logic

### Testing & Documentation
- [examples/m3_mudlib/user.c](../../examples/m3_mudlib/user.c) - Added shutdown command
- [examples/testbot.py](../../examples/testbot.py) - Updated for cross-platform support
- [docs/plan/console-testbot-support.md](../../docs/plan/console-testbot-support.md) - Marked Phase 2 complete
- [docs/ChangeLog.md](../../docs/ChangeLog.md) - Documented Phase 2 completion
- [docs/manual/console-mode.md](../../docs/manual/console-mode.md) - Updated for Windows pipe support

## Next Steps (Phase 3)

Phase 3 checklist for documentation and testing:
- ⏳ Update io-reactor.md with Windows pipe implementation details
- ⏳ Create integration test scripts for both platforms
- ⏳ Add examples of testbot.py usage patterns
- ⏳ Archive Phase 1 and Phase 2 implementation reports

**Estimated Effort**: 2-3 hours for Phase 3 completion

## Lessons Learned

1. **Simplicity over complexity**: The synchronous ReadFile() approach works perfectly and is much simpler than overlapped I/O
2. **Trust the pattern**: If Phase 1 works with simple blocking reads, Phase 2 should too
3. **IOCP limitations**: IOCP completion handles are not waitable objects - can't be used with `WaitForMultipleObjects()`
4. **Test-driven fixes**: Unit test failures revealed fundamental IOCP architecture issues that needed fixing anyway
5. **Platform parity**: Windows should mirror POSIX design - use sync I/O for stdin, not async

## References

- [Console Testbot Support Plan](../../docs/plan/console-testbot-support.md)
- [Console Mode Documentation](../../docs/manual/console-mode.md)
- [testbot.py](../../examples/testbot.py)
