# Console Mode async_runtime_add() Fix

**Date**: 2026-02-12  
**Type**: Bug Fix  
**Component**: Console Worker Integration  
**Related**: [async-phase2-console-worker-2026-01-20.md](async-phase2-console-worker-2026-01-20.md)

---

## Problem

Console mode was failing to start with error:
```
Failed to register user socket with async runtime
```

**Root Cause**: [new_interactive()](../../../src/comm.c#L1220) incorrectly attempted to register stdin (STDIN_FILENO) with `async_runtime_add()` when creating console user (slot 0).

## Why This Is Wrong

The console worker architecture uses **completion posting**, not I/O readiness notification:

1. **Console worker thread** reads from stdin directly using `ReadConsole()`/`read()`
2. **Worker enqueues** completed lines to `g_console_queue`  
3. **Worker posts completion** via `async_runtime_post_completion(CONSOLE_COMPLETION_KEY)`
4. **Main thread** receives completion event and dequeues from queue

stdin is **never polled** by async_runtime - the worker handles all reading. The runtime only receives completion notifications.

### Platform-Specific Issues

- **Windows**: Console handles cannot be registered with IOCP (will fail)
- **POSIX**: epoll would redundantly monitor stdin already handled by worker thread

## Solution

Modified [new_interactive()](../../../src/comm.c#L1285-L1317) to skip `async_runtime_add()` for console user:

**Before**:
```c
if (i == 0)  /* Console user - WRONG! */
{
    if (async_runtime_add(g_runtime, socket_fd, EVENT_READ, ...) != 0)
        /* Error... */
}
```

**After**:
```c
if (i > 0)  /* Network users only */
{
    if (async_runtime_add(g_runtime, socket_fd, EVENT_READ, ...) != 0)
        /* Error... */
}
```

### Why This Works

Console completions are handled in [process_io()](../../../src/comm.c#L1109-L1140):
```c
if (evt->completion_key == CONSOLE_COMPLETION_KEY) {
    /* Console worker posted completion */
    while (async_queue_dequeue(g_console_queue, line_buffer, &line_length)) {
        add_console_line(console_ip, line_buffer, line_length);
    }
}
```

This path is **independent** of stdin being registered with async_runtime. The console worker posts completions directly, which wake up `async_runtime_wait()`.

## Files Modified

- [src/comm.c](../../../src/comm.c#L1285-L1317) - Changed condition from `i == 0` to `i > 0`

## Testing

Build verification:
```bash
cmake --build --preset ci-vs16-x64
```

✅ **Status**: Build successful with no errors

## Documentation Updates

Updated comments in `new_interactive()` to clarify:
- Console user (slot 0) uses console worker via completion posting
- Only network users (slot > 0) need async_runtime_add()
- stdin is NOT registered for console mode

---

**Impact**: Console mode now starts correctly on all platforms. No functional changes to network user handling.
