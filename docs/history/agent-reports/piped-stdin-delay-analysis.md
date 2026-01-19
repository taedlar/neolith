# Piped Stdin Response Delay Analysis

**Date**: 2026-01-17  
**Issue**: Multi-second delays between commands in testbot.py

## Root Cause

The delay is caused by **two separate issues** depending on platform:

### Issue 1: Backend Timeout (Both Platforms)

**Location**: [src/backend.c](../../../src/backend.c#L311-L316)

```c
if (heart_beat_flag || has_pending_commands)
{
    /* When heart beat is active or commands pending, do not wait in poll */
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
}
else
{
    /* When heart beat is not active and no pending commands, wait up to 60 seconds */
    timeout.tv_sec = 60;  // ⚠️ THIS CAUSES 60-SECOND DELAYS
    timeout.tv_usec = 0;
}
```

**Problem**: When there are no pending commands and no heartbeat, the backend waits up to **60 seconds** for I/O events.

**Why this happens**:
1. User sends command via pipe
2. Pipe data available, but no command in buffer yet
3. Backend calls `io_reactor_wait()` with 60-second timeout
4. Reactor returns EVENT_READ
5. `get_user_data()` reads data, buffers it
6. Command is now in buffer
7. Next backend loop iteration sees `has_pending_commands = 1`
8. Now uses 0 timeout for fast processing

**Impact**: **First command after idle period** experiences up to 60-second delay.

### Issue 2: Windows Blocking ReadFile() (Windows Only)

**Location**: [src/comm.c](../../../src/comm.c#L1732)

```c
else if (console_type == CONSOLE_TYPE_PIPE || console_type == CONSOLE_TYPE_FILE) {
    /* Pipe or file: use synchronous ReadFile (no overlapped I/O needed) */
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD bytes_read;
    if (ReadFile(hStdin, buf, text_space, &bytes_read, NULL)) {
        num_bytes = bytes_read;
    }
    // ...
}
```

**Problem**: `ReadFile()` on a pipe is **synchronous and blocking** - it waits until data is available.

**Why this is wrong**:
1. IO reactor marks pipe as "always ready" ([io_reactor_win32.c](../../../lib/port/io_reactor_win32.c#L577-L583))
2. Backend calls `process_io()` immediately
3. `get_user_data()` calls blocking `ReadFile()`
4. If no data, **blocks until data arrives** (could be forever)

**Expected behavior**: Should use `PeekNamedPipe()` to check availability first.

**Microsoft Documentation Findings**:
- **Anonymous pipes CANNOT be waited on with `WaitForMultipleObjects()`**
- Per [WaitForMultipleObjects documentation](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitformultipleobjects), supported waitable objects are:
  - Change notification, **Console input**, Event, Memory resource notification, Mutex, Process, Semaphore, Thread, Waitable timer
  - **Pipes are NOT in this list**
- `PeekNamedPipe()` is the documented way to check anonymous pipe availability
- Per [PeekNamedPipe documentation](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-peeknamedpipe):
  - Works with "read end of an anonymous pipe, as returned by CreatePipe function"
  - "Always returns immediately in a single-threaded application"
  - Returns total bytes available without removing data from pipe

**Solution**: Use `PeekNamedPipe()` before signaling EVENT_READ (implemented in Solution B)

### Issue 3: POSIX Works Correctly (Reference)

**Location**: [src/backend.c](../../../src/backend.c#L168)

```c
#ifdef HAVE_TERMIOS_H
{
    struct termios tio;
    tcgetattr (STDIN_FILENO, &tio);
    tio.c_lflag |= ICANON | ECHO;
    tio.c_cc[VMIN] = 0;  // ✅ Use polling (non-blocking)
    tio.c_cc[VTIME] = 0; // ✅ No timeout
    tcsetattr (STDIN_FILENO, action, &tio);
}
#endif
```

**Why POSIX works**: `VMIN=0, VTIME=0` makes `read()` non-blocking - returns immediately with available data or EWOULDBLOCK.

## Observed Behavior

When running `testbot.py`:
- **Expected**: Commands processed instantly (< 100ms)
- **Actual**: 1-60 second delays between commands

**testbot.py timing**:
```python
child.sendline("say Hello from Python test!")
child.expect('You say: ...', timeout=5)  # ⚠️ May timeout due to 60s backend delay
```

The `pexpect` library has a 5-second timeout, which may or may not catch the response depending on when the 60-second backend wait expires.

## Solutions

### Solution A: Reduce Backend Idle Timeout (Quick Fix)

Change the 60-second timeout to something reasonable like 1 second:

```c
else
{
    /* When heart beat is not active and no pending commands, wait up to 1 second */
    timeout.tv_sec = 1;   // Changed from 60
    timeout.tv_usec = 0;
}
```

**Pros**: Simple one-line change, fixes both issues
**Cons**: Still has up to 1-second delay on first command
**Status**: Not implemented

### Solution B: Check Console Input Availability (Proper Fix) - ✅ IMPLEMENTED

Make Windows pipe checking actually check for data availability:

```c
else if (reactor->console_type == CONSOLE_TYPE_PIPE) {
    /* Pipe: check if data available before signaling ready */
    DWORD bytes_available = 0;
    BOOL result = PeekNamedPipe(reactor->console_handle, NULL, 0, NULL, &bytes_available, NULL);
    if (result && bytes_available > 0) {
        events[event_count].context = reactor->console_context;
        events[event_count].event_type = EVENT_READ;
        events[event_count].buffer = NULL;
        events[event_count].bytes_transferred = 0;
        event_count++;
    }
}
```

**Pros**: Correct behavior, no spurious wake-ups
**Cons**: Requires code changes in reactor and potentially comm.c
**Status**: ✅ **Implemented in [lib/port/io_reactor_win32.c](../../../lib/port/io_reactor_win32.c)**

**Implementation Details**:
- Modified `io_reactor_wait()` to use `PeekNamedPipe()` for `CONSOLE_TYPE_PIPE`
- Only signals `EVENT_READ` when data is actually available
- File handles (`CONSOLE_TYPE_FILE`) still marked as always ready (correct behavior)
- Prevents blocking `ReadFile()` calls when no data present

### Solution C: Set Backend Timeout Based on Console Activity

When console is enabled, use shorter timeout:

```c
if (heart_beat_flag || has_pending_commands)
{
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
}
else if (MAIN_OPTION(console_mode))
{
    /* Console mode: poll more frequently for user input */
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;  // 100ms
}
else
{
    /* Network only: can wait longer */
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;
}
```

**Pros**: Balances responsiveness with CPU usage
**Cons**: Console mode uses more CPU (but acceptable for testing)
**Status**: Not implemented (Solution B may be sufficient)

## Recommendations

**Implemented**: Solution B (PeekNamedPipe checking) ✅
- Prevents spurious EVENT_READ signals on Windows pipes
- Eliminates race condition with blocking ReadFile()
- Correct behavior: only signals when data actually available

**Remaining Issue**: Backend 60-second timeout still affects first command after idle
- Consider implementing Solution C (console mode timeout) for better responsiveness
- Or Solution A (reduce general timeout to 1 second) as fallback

**Testing Needed**:
- Verify testbot.py performance on Windows improves
- Check that pipe EOF still detected correctly
- Ensure no regression in real console mode

## Impact Assessment

**Current Impact**: 
- Automated testing appears slow/flaky
- Interactive console users see delays after idle periods
- May cause pexpect timeout failures

**After Fix**:
- Sub-100ms command response time
- Reliable automated testing
- Better interactive UX

## Related Files

- [src/backend.c](../../../src/backend.c) - Main event loop
- [src/comm.c](../../../src/comm.c) - Console input reading
- [lib/port/io_reactor_win32.c](../../../lib/port/io_reactor_win32.c) - Windows reactor
- [examples/testbot.py](../../../examples/testbot.py) - Automated test experiencing delays
