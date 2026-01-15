# Console Mode Testing Robot Support - Unified Design

## Executive Summary

Enable platform-agnostic automated testing via `testbot.py` by supporting piped stdin on both Windows and POSIX platforms while preserving optimal behavior for real terminal consoles.

**Current Status**:
- ✅ **Linux/WSL Real TTY**: Works correctly with termios control
- ✅ **Linux/WSL Piped Stdin**: Works correctly (Phase 1 complete)
- ✅ **Windows Real Console**: Works correctly with ReadConsoleInputW()
- ✅ **Windows Piped Stdin**: Works correctly (Phase 2 complete)

**Goal**: `testbot.py` works identically on both platforms with piped stdin while maintaining security/UX for real terminals.

## Problem Analysis

### Platform-Specific Issues

| Platform | Input Type | Before | After (Current) |
|----------|-----------|--------|-----------------|
| **Linux/WSL** | Real TTY | ✅ Works | ✅ Works (unchanged) |
| **Linux/WSL** | Pipe | ❌ Data lost (TCSAFLUSH) | ✅ Works (Phase 1) |
| **Windows** | Real Console | ✅ Works | ✅ Works (unchanged) |
| **Windows** | Pipe | ❌ Rejected (GetConsoleMode) | ✅ Works (Phase 2) |

### Root Causes

**POSIX (Linux/WSL)**:
- `tcsetattr(fd, TCSAFLUSH, &tio)` discards input buffer
- Used in 4 locations: init, single-char mode, password input, echo restoration
- Correct for interactive terminals (security), wrong for pipes (data loss)

**Windows**:
- Console-specific APIs: `GetConsoleMode()`, `GetNumberOfConsoleInputEvents()`, `ReadConsoleInputW()`
- Pipes have different handle type (`FILE_TYPE_PIPE` vs `FILE_TYPE_CHAR`)
- No overlapped I/O implemented for pipe handles

### Common Pattern

Both platforms need **handle type detection** to dispatch to appropriate I/O methods:

```
┌─────────────────┐
│   stdin handle  │
└────────┬────────┘
         │
    Detect type
         │
    ┌────┴────┐
    │         │
  TTY/Console  Pipe
    │         │
 Platform    Platform
 optimized   generic
 terminal    I/O
 control     preservation
```

## Unified Design Solution

### Design Principle: Hybrid Dispatch

**For Real Terminals** (TTY/Console):
- Optimize for human interaction
- Use platform-specific APIs for best UX
- Flush input on mode changes (security)

**For Pipes** (Testing Robots):
- Preserve all input data
- Use generic I/O without flushing
- Treat as data stream, not interactive session

### Detection Strategy

| Platform | Detection Method | Implementation |
|----------|-----------------|----------------|
| **POSIX** | `isatty(fd)` | Returns 1 for TTY, 0 for pipes/files |
| **Windows** | `GetFileType(handle)` | Returns `FILE_TYPE_CHAR` (console) vs `FILE_TYPE_PIPE` |

### Platform-Specific Implementations

#### POSIX: Conditional TCSAFLUSH

```c
#ifdef HAVE_TERMIOS_H
/**
 * @brief Apply terminal settings without data loss for pipes
 * 
 * Real TTY: Use TCSAFLUSH to discard stale input (security)
 * Pipe:     Use TCSANOW to preserve all data (testbot)
 */
static inline void safe_tcsetattr(int fd, struct termios *tio) {
    int action = isatty(fd) ? TCSAFLUSH : TCSANOW;
    tcsetattr(fd, action, tio);
}
#endif
```

**Changes Required**:
- Replace `tcsetattr(fd, TCSAFLUSH, &tio)` → `safe_tcsetattr(fd, &tio)` in:
  - [backend.c:171](../../src/backend.c#L171) - `init_console_user()`
  - [comm.c:953](../../src/comm.c#L953) - `set_telnet_single_char()`
  - [comm.c:2340](../../src/comm.c#L2340) - `set_call()` password input
  - [comm.c:1967](../../src/comm.c#L1967) - echo restoration

**Estimated Effort**: 30 minutes

#### Windows: Handle Type Dispatch

```c
typedef enum {
    CONSOLE_TYPE_NONE = 0,
    CONSOLE_TYPE_REAL,    // Real console (ReadConsoleInputW)
    CONSOLE_TYPE_PIPE,    // Pipe (overlapped ReadFile)
    CONSOLE_TYPE_FILE     // File (synchronous ReadFile, always ready)
} console_type_t;

struct io_reactor_s {
    console_type_t console_type;
    HANDLE console_handle;
    HANDLE console_event;         // Event for pipe overlapped I/O
    iocp_context_t *console_ctx;  // Buffer + OVERLAPPED for pipes
    void *console_context;
    int console_enabled;
};
```

**Detection Logic** ([lib/port/io_reactor_win32.c](../../lib/port/io_reactor_win32.c)):
```c
int io_reactor_add_console(io_reactor_t *reactor, void *context) {
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD fileType = GetFileType(hStdin);
    DWORD mode;
    
    if (fileType == FILE_TYPE_CHAR && GetConsoleMode(hStdin, &mode)) {
        // Real console: use GetNumberOfConsoleInputEvents() approach
        reactor->console_type = CONSOLE_TYPE_REAL;
        reactor->console_handle = hStdin;
        reactor->console_enabled = 1;
    }
    else if (fileType == FILE_TYPE_PIPE) {
        // Pipe: use overlapped ReadFile with event
        reactor->console_type = CONSOLE_TYPE_PIPE;
        reactor->console_handle = hStdin;
        reactor->console_event = CreateEvent(NULL, TRUE, FALSE, NULL);
        
        // Allocate context and start async read
        reactor->console_ctx = calloc(1, sizeof(iocp_context_t));
        reactor->console_ctx->overlapped.hEvent = reactor->console_event;
        
        BOOL result = ReadFile(hStdin, reactor->console_ctx->buffer,
                              sizeof(reactor->console_ctx->buffer),
                              NULL, &reactor->console_ctx->overlapped);
        if (!result && GetLastError() != ERROR_IO_PENDING) {
            CloseHandle(reactor->console_event);
            free(reactor->console_ctx);
            return -1;
        }
        reactor->console_enabled = 1;
    }
    else if (fileType == FILE_TYPE_DISK) {
        // File: synchronous reads (always ready, no event needed)
        reactor->console_type = CONSOLE_TYPE_FILE;
        reactor->console_handle = hStdin;
        reactor->console_enabled = 1;
    }
    else {
        return -1;  // Unsupported handle type
    }
    
    reactor->console_context = context;
    return 0;
}
```

**Reading Logic** ([src/comm.c](../../src/comm.c)):
```c
#ifdef _WIN32
void get_user_data() {
    if (reactor->console_type == CONSOLE_TYPE_REAL) {
        // Existing: ReadConsoleInputW() + UTF-8 conversion
        INPUT_RECORD ir;
        DWORD count;
        ReadConsoleInputW(stdin_handle, &ir, 1, &count);
        // ... process key events ...
    }
    else if (reactor->console_type == CONSOLE_TYPE_PIPE) {
        // New: overlapped result retrieval
        DWORD bytes_read;
        if (GetOverlappedResult(reactor->console_handle,
                                &reactor->console_ctx->overlapped,
                                &bytes_read, FALSE)) {
            if (bytes_read == 0) {
                // EOF - pipe closed
                remove_interactive(command_giver, 0);
            } else {
                // Process data
                add_to_input_buffer(reactor->console_ctx->buffer, bytes_read);
                
                // Post next async read
                ResetEvent(reactor->console_event);
                ReadFile(reactor->console_handle, reactor->console_ctx->buffer,
                        sizeof(reactor->console_ctx->buffer),
                        NULL, &reactor->console_ctx->overlapped);
            }
        }
    }
    else if (reactor->console_type == CONSOLE_TYPE_FILE) {
        // Synchronous read (file always ready)
        char buffer[4096];
        DWORD bytes_read;
        if (ReadFile(reactor->console_handle, buffer, sizeof(buffer),
                    &bytes_read, NULL)) {
            if (bytes_read == 0) {
                // EOF
                remove_interactive(command_giver, 0);
            } else {
                add_to_input_buffer(buffer, bytes_read);
            }
        }
    }
}
#endif
```

**Reactor Wait Logic**:
```c
int io_reactor_wait(io_reactor_t *reactor, int timeout_ms) {
    DWORD count = reactor->num_events;
    
    // Add console event if pipe mode
    if (reactor->console_enabled && 
        reactor->console_type == CONSOLE_TYPE_PIPE) {
        reactor->event_array[count++] = reactor->console_event;
    }
    
    DWORD result = WaitForMultipleObjects(count, reactor->event_array,
                                          FALSE, timeout_ms);
    
    // Check console availability
    if (reactor->console_enabled) {
        if (reactor->console_type == CONSOLE_TYPE_REAL) {
            // Existing: GetNumberOfConsoleInputEvents()
            DWORD num_events;
            if (GetNumberOfConsoleInputEvents(reactor->console_handle, &num_events) 
                && num_events > 0) {
                events |= EVENT_CONSOLE;
            }
        }
        else if (reactor->console_type == CONSOLE_TYPE_PIPE) {
            // Check if pipe event signaled
            if (result >= WAIT_OBJECT_0 && 
                result < WAIT_OBJECT_0 + count) {
                DWORD index = result - WAIT_OBJECT_0;
                if (reactor->event_array[index] == reactor->console_event) {
                    events |= EVENT_CONSOLE;
                }
            }
        }
        else if (reactor->console_type == CONSOLE_TYPE_FILE) {
            // File always ready (or EOF)
            events |= EVENT_CONSOLE;
        }
    }
    
    return events;
}
```

**Estimated Effort**: 4-6 hours
- 2 hours: Reactor detection and event management
- 2 hours: Reading logic in comm.c
- 1 hour: Testing with pipes and files
- 1 hour: Documentation updates

## Implementation Phases

### Phase 1: POSIX Pipe Support (Quick Win)
**Goal**: Enable testbot.py on Linux/WSL

1. Add `safe_tcsetattr()` helper to [src/comm.c](../../src/comm.c)
2. Replace 4 `tcsetattr(TCSAFLUSH)` calls
3. Test with testbot.py on Linux/WSL
4. Verify interactive console still works correctly

**Effort**: ~30 minutes  
**Deliverable**: testbot.py works on Linux/WSL

### Phase 2: Windows Pipe Support
**Goal**: Enable testbot.py on Windows

1. Extend `io_reactor_s` structure with console_type, console_event, console_ctx
2. Modify `io_reactor_add_console()` for handle type detection
3. Update `io_reactor_wait()` to handle pipe events
4. Modify `get_user_data()` in comm.c for overlapped result retrieval
5. Add cleanup in `io_reactor_destroy()`
6. Test with testbot.py on Windows
7. Verify real console still works correctly

**Effort**: 4-6 hours  
**Deliverable**: testbot.py works on Windows

### Phase 3: Documentation and Testing
**Goal**: Complete platform-agnostic testbot infrastructure

1. Update [docs/manual/console-mode.md](../../docs/manual/console-mode.md)
2. Document testbot.py usage patterns
3. Add unit tests for pipe/file input on both platforms
4. Create integration test suite
5. Update [docs/ChangeLog.md](../../docs/ChangeLog.md)

**Effort**: 2-3 hours  
**Deliverable**: Complete documentation and test coverage

## Platform Behavior Matrix

| Input Type | Linux/WSL Before | Linux/WSL After | Windows Before | Windows After |
|------------|------------------|-----------------|----------------|---------------|
| **Real TTY/Console** | ✅ Works | ✅ Works (same) | ✅ Works | ✅ Works (same) |
| **Piped stdin** | ❌ Data lost | ✅ Works | ❌ Rejected | ✅ Works |
| **File redirect** | ❌ Data lost | ✅ Works | ❌ Rejected | ✅ Works |
| **Interactive UX** | ✅ Optimal | ✅ Optimal | ✅ Optimal | ✅ Optimal |
| **Security (flush)** | ✅ Yes | ✅ Yes (TTY only) | ✅ Yes | ✅ Yes (console only) |

## Testing Strategy

### Unit Tests

**POSIX**:
```c
TEST(ConsoleMode, PipedStdinPreservesInput) {
    // Create pipe
    int pipefd[2];
    pipe(pipefd);
    
    // Write test commands
    write(pipefd[1], "wizard\nwizard\nquit\n", 21);
    close(pipefd[1]);
    
    // Redirect stdin to pipe
    dup2(pipefd[0], STDIN_FILENO);
    
    // Initialize console user
    init_console_user(0);
    
    // Verify input not flushed
    char buffer[1024];
    ssize_t bytes = read(STDIN_FILENO, buffer, sizeof(buffer));
    EXPECT_GT(bytes, 0);
    EXPECT_STREQ(buffer, "wizard\nwizard\nquit\n");
    
    close(pipefd[0]);
}
```

**Windows**:
```c
TEST(ConsoleMode, PipedStdinSupported) {
    HANDLE read_pipe, write_pipe;
    CreatePipe(&read_pipe, &write_pipe, NULL, 0);
    
    // Write test commands
    DWORD written;
    WriteFile(write_pipe, "wizard\r\nwizard\r\nquit\r\n", 24, &written, NULL);
    CloseHandle(write_pipe);
    
    // Set stdin to pipe
    SetStdHandle(STD_INPUT_HANDLE, read_pipe);
    
    // Should successfully register
    io_reactor_t *reactor = io_reactor_create();
    int result = io_reactor_add_console(reactor, NULL);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(reactor->console_type, CONSOLE_TYPE_PIPE);
    
    io_reactor_destroy(reactor);
    CloseHandle(read_pipe);
}
```

### Integration Tests

**Linux/WSL**:
```bash
#!/bin/bash
cd examples

# Test with pipe
echo -e "wizard\nwizard\nsay test\nquit" | ../out/build/linux/src/RelWithDebInfo/neolith -f m3.local.conf -c
if [ $? -eq 0 ]; then
    echo "✅ Pipe test passed"
else
    echo "❌ Pipe test failed"
    exit 1
fi

# Test with file
echo -e "wizard\nwizard\nsay test\nquit" > commands.txt
../out/build/linux/src/RelWithDebInfo/neolith -f m3.local.conf -c < commands.txt
if [ $? -eq 0 ]; then
    echo "✅ File redirect test passed"
else
    echo "❌ File redirect test failed"
    exit 1
fi
rm commands.txt

# Test with Python testbot
python3 testbot.py
if [ $? -eq 0 ]; then
    echo "✅ testbot.py passed"
else
    echo "❌ testbot.py failed"
    exit 1
fi
```

**Windows PowerShell**:
```powershell
# Test with pipe
"wizard`nwizard`nsay test`nquit" | .\out\build\vs16-x64\src\RelWithDebInfo\neolith.exe -f examples\m3.local.conf -c
if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ Pipe test passed" -ForegroundColor Green
} else {
    Write-Host "❌ Pipe test failed" -ForegroundColor Red
    exit 1
}

# Test with file
"wizard`nwizard`nsay test`nquit" | Out-File -Encoding ASCII commands.txt
.\out\build\vs16-x64\src\RelWithDebInfo\neolith.exe -f examples\m3.local.conf -c < commands.txt
if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ File redirect test passed" -ForegroundColor Green
} else {
    Write-Host "❌ File redirect test failed" -ForegroundColor Red
    exit 1
}
Remove-Item commands.txt

# Test with Python testbot
cd examples
python testbot.py
if ($LASTEXITCODE -eq 0) {
    Write-Host "✅ testbot.py passed" -ForegroundColor Green
} else {
    Write-Host "❌ testbot.py failed" -ForegroundColor Red
    exit 1
}
```

## Security Considerations

### Input Flushing Behavior

**Real Terminals** (unchanged):
- Mode switches flush input buffer (prevent injection attacks)
- Password prompts flush pending input (prevent shoulder surfing)
- Reconnection flushes ENTER key (clean state)

**Piped Input** (new):
- No flushing (all data preserved)
- No interactive password prompts (testing scenarios)
- No reconnection (automated exit on error)

### Attack Surface Analysis

**Before (real terminals only)**:
- Human types commands interactively
- Input validated one line at a time
- Mode switches discard stale data

**After (pipes supported)**:
- Scripts can pipe commands non-interactively
- All input pre-validated by script author
- No interactive password prompts in pipe mode

**Risk**: Low - piped input implies trusted source (developer's own test scripts)

**Mitigation**: Document that console mode is for **development/testing only**, not production

## Performance Impact

### POSIX
- `isatty()` syscall: ~1µs overhead per mode switch (negligible)
- Total: 4 calls during session lifecycle

### Windows
- `GetFileType()`: ~1µs overhead at startup
- Overlapped I/O: same performance as network sockets (already optimized)
- Console mode: unchanged (no overhead)

**Conclusion**: Performance impact negligible on both platforms.

## Backward Compatibility

### Code Changes
- ✅ No API changes to public interfaces
- ✅ No config file changes
- ✅ No LPC API changes

### Behavior Changes
- ✅ Real terminals: identical behavior
- ✅ Piped stdin: new functionality (previously rejected/broken)

### Testing Impact
- ✅ Existing tests continue to work
- ✅ New tests added for pipe scenarios

## Success Criteria

**All criteria met** ✅:

1. ✅ `testbot.py` executes successfully on Linux/WSL with piped stdin
2. ✅ `testbot.py` executes successfully on Windows with piped stdin
3. ✅ Real console interaction unchanged on both platforms
4. ✅ Password input (NOECHO) works correctly in both modes
5. ✅ Single-character mode works correctly in both modes
6. ✅ All existing unit tests pass (37/37 io_reactor tests)
7. ✅ Pipe EOF triggers clean shutdown instead of reconnect loop
8. ✅ Documentation updated with platform-agnostic testing guide

## Implementation Checklist

### Phase 1: POSIX (Linux/WSL) - ✅ COMPLETE
- [x] Add `safe_tcsetattr()` to [src/comm.c](../../src/comm.c)
- [x] Update [src/backend.c](../../src/backend.c#L171)
- [x] Update [src/comm.c](../../src/comm.c#L953)
- [x] Update [src/comm.c](../../src/comm.c#L2340)
- [x] Update [src/comm.c](../../src/comm.c#L1967)
- [x] Test with piped stdin on Linux/WSL
- [x] Test interactive console on Linux/WSL
- [x] Update [testbot.py](../../examples/testbot.py) for automated testing

### Phase 2: Windows - ✅ COMPLETE
- [x] Add `console_type_t` enum to [lib/port/io_reactor.h](../../lib/port/io_reactor.h)
- [x] Extend `struct io_reactor_s` with console_type field
- [x] Modify `io_reactor_add_console()` for type detection
- [x] Modify `io_reactor_wait()` for IOCP-compatible event handling
- [x] Modify `get_user_data()` in [src/comm.c](../../src/comm.c) for synchronous ReadFile
- [x] Add pipe EOF detection and clean shutdown
- [x] Fix IOCP reactor wait timeout handling
- [x] Fix io_reactor_wakeup() to signal both event and IOCP
- [x] Test with `testbot.py` on Windows
- [x] Test real console on Windows
- [x] Fix all io_reactor unit tests (37/37 passing)

### Phase 3: Documentation - ✅ COMPLETE
- [x] Update [docs/manual/console-mode.md](../../docs/manual/console-mode.md)
- [x] Add testbot.py usage examples
- [x] Update [docs/ChangeLog.md](../../docs/ChangeLog.md)
- [x] Create implementation report [docs/history/agent-reports/console-testbot-phase2.md](../../docs/history/agent-reports/console-testbot-phase2.md)
- [x] Update testbot.py documentation
- [x] Archive/reference this design document

## Project Complete

**Status**: ✅ All three phases complete

1. **✅ Phase 1 (POSIX)**: Conditional TCSAFLUSH based on isatty() detection
2. **✅ Phase 2 (Windows)**: Handle type detection with synchronous ReadFile() for pipes
3. **✅ Phase 3 (Documentation)**: All documentation updated and implementation archived

**Achievement**: Cross-platform testbot.py automation working on both Linux/WSL and Windows.

---

**Related Documents**:
- [console-mode.md](../../docs/manual/console-mode.md) - User manual
- [io-reactor.md](../../docs/manual/io-reactor.md) - IO reactor design
- [testbot.py](../../examples/testbot.py) - Testing robot template
