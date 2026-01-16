# Enhancement Plan: Console Worker Thread with IOCP Completion Notification

**Status**: Proposed  
**Target Platform**: Windows  
**Created**: 2026-01-02  
**Updated**: 2026-01-17 (Completion notification design)  
**Related**: [io-reactor-phase3-console-support.md](agent-reports/io-reactor-phase3-console-support.md), [windows-io.md](../manual/windows-io.md), [piped-stdin-delay-analysis.md](agent-reports/piped-stdin-delay-analysis.md)

## Problem Statement

Current Windows console implementation has two issues:

**Issue 1: Console Mode** (Phase 3 - CONSOLE_TYPE_REAL)
- Uses `ReadConsoleInputW()` in raw mode (character-by-character)
- Mudlib provides command editing via LPC code
- **Missing**: Native Windows console features (backspace, arrow keys, command history via F7, etc.)

**Issue 2: Piped/File stdin** (CONSOLE_TYPE_PIPE, CONSOLE_TYPE_FILE)
- Current polling approach uses `PeekNamedPipe()` to check availability
- Still subject to backend 60-second timeout (reactor polls only when timeout expires)
- Commands can take up to 60 seconds to process even when data is ready

While functional and non-blocking, these approaches lose Windows console capabilities and suffer from delayed responsiveness.

## Proposed Solution

Implement **worker thread with IOCP completion notification** for all console input types:

```
┌──────────────────────────────────────────────────────────────┐
│ Main Thread (Backend Event Loop)                             │
│  ┌────────────────────────────────────────────────────┐     │
│  │ GetQueuedCompletionStatus(iocp_handle, ...)       │     │
│  │   - Network I/O completions (existing)            │     │
│  │   - Console worker completions (NEW!)      ◄──────┼─────┐│
│  │   - Timeout only when queue empty                 │     ││
│  └────────────────────────────────────────────────────┘     ││
│                     ▲                                        ││
│                     │ (thread-safe queue)                    ││
│                     │                                        ││
└─────────────────────┼────────────────────────────────────────┘│
                      │                                         │
┌─────────────────────┼─────────────────────────────────────────┘
│ Console Worker Thread (All stdin types)                      │
│  ┌────────────────────────────────────────────────────┐     │
│  │ while (running) {                                  │     │
│  │   DWORD bytes_read = 0;                            │     │
│  │                                                     │     │
│  │   if (console_type == CONSOLE_TYPE_REAL) {         │     │
│  │     // Native line editing for interactive console │     │
│  │     ReadConsole(stdin, buf, size, &bytes_read, ..);│ ◄───┼─ BLOCKS
│  │   } else {                                         │     │
│  │     // Pipes and files (testbot.py, redirected I/O)│     │
│  │     ReadFile(stdin, buf, size, &bytes_read, NULL); │ ◄───┼─ BLOCKS
│  │   }                                                │     │
│  │                                                     │     │
│  │   if (bytes_read > 0) {                            │     │
│  │     EnqueueLine(queue, buf, bytes_read);          │     │
│  │                                                     │     │
│  │     // POST COMPLETION - wakes main thread INSTANTLY!    │
│  │     PostQueuedCompletionStatus(                    │     │
│  │       iocp_handle,                                 │     │
│  │       bytes_read,                                  │     │
│  │       (ULONG_PTR)CONSOLE_COMPLETION_KEY,          │     │
│  │       NULL);  ──────────────────────────────────────────┘│
│  │   }                                                 │     │
│  │ }                                                   │     │
│  └─────────────────────────────────────────────────────┘     │
└───────────────────────────────────────────────────────────────┘
```

**Key Innovation:** Console input becomes just another **completion source** in the IOCP queue, alongside network I/O completions.

### Architecture Components

**1. Thread-Safe Line Queue** (new: `lib/port/console_queue.c`)
```c
typedef struct {
    CRITICAL_SECTION lock;
    char* lines[QUEUE_SIZE];        // Circular buffer of completed lines
    size_t head, tail, count;
    HANDLE data_ready_event;        // Manual-reset event for signaling
} console_queue_t;

void cq_init(console_queue_t* q);
bool cq_enqueue(console_queue_t* q, const char* line, size_t len);
bool cq_dequeue(console_queue_t* q, char* buf, size_t max_len, size_t* out_len);
void cq_destroy(console_queue_t* q);
```

**2. Console Worker Thread** (new: `lib/port/console_worker.c`)
```c
typedef enum {
    CONSOLE_TYPE_REAL,   // Interactive console (ReadConsole)
    CONSOLE_TYPE_PIPE,   // Piped stdin (testbot.py)
    CONSOLE_TYPE_FILE    // File redirection
} console_type_t;

typedef struct {
    HANDLE thread_handle;
    HANDLE shutdown_event;          // Signal worker to exit
    HANDLE iocp_handle;             // IOCP for posting completions
    console_queue_t* queue;
    console_type_t console_type;
    volatile bool running;
} console_worker_t;

#define CONSOLE_COMPLETION_KEY ((ULONG_PTR)0xC0701E)  // Unique key

DWORD WINAPI console_thread_proc(LPVOID param) {
    console_worker_t* worker = (console_worker_t*)param;
    HANDLE stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
    
    while (worker->running) {
        char buf[4096];
        DWORD bytes_read = 0;
        BOOL success = FALSE;
        
        // Select read method based on console type
        if (worker->console_type == CONSOLE_TYPE_REAL) {
            // Interactive console - native line editing
            success = ReadConsole(stdin_handle, buf, sizeof(buf) - 1, 
                                 &bytes_read, NULL);
        } else {
            // Pipes and files - synchronous ReadFile
            success = ReadFile(stdin_handle, buf, sizeof(buf) - 1, 
                              &bytes_read, NULL);
        }
        
        if (!success) {
            break;  // Console closed or error
        }
        
        if (bytes_read > 0) {
            buf[bytes_read] = '\0';
            cq_enqueue(worker->queue, buf, bytes_read);
            
            // POST COMPLETION TO IOCP - wakes main thread INSTANTLY
            // This is the key: main thread's GetQueuedCompletionStatus()
            // returns immediately with this completion, regardless of timeout
            PostQueuedCompletionStatus(
                worker->iocp_handle,
                bytes_read,                      // Bytes transferred
                CONSOLE_COMPLETION_KEY,          // Completion key (identifies console)
                NULL                             // No OVERLAPPED needed
            );
        }
    }
    return 0;
}

bool console_worker_init(console_worker_t* worker, console_queue_t* queue, 
                        HANDLE iocp, console_type_t type);
void console_worker_shutdown(console_worker_t* worker);
```

**3. Reactor Integration** (modify: `lib/port/io_reactor_win32.c`)
```c
int io_reactor_wait(io_reactor_t* reactor, io_event_t* events, 
                    int max_events, int timeout_ms) {
    DWORD num_bytes;
    ULONG_PTR completion_key;
    LPOVERLAPPED overlapped;
    
    // This blocks up to timeout_ms (e.g., 60,000ms)
    // BUT gets woken up by:
    // 1. Network I/O completions (existing)
    // 2. Console worker completions (NEW!) ← Solves backend timeout!
    BOOL ok = GetQueuedCompletionStatus(
        reactor->iocp_handle,
        &num_bytes,
        &completion_key,
        &overlapped,
        timeout_ms
    );
    
    if (completion_key == CONSOLE_COMPLETION_KEY) {
        // Console worker posted completion - data ready in queue!
        events[count].events = EVENT_READ;
        events[count].fd = STDIN_FILENO;
        events[count].context = reactor->console_context;
        count++;
    } else {
        // Network I/O completion (existing code)
        // ... handle AcceptEx, WSARecv, WSASend completions ...
    }
    
    return count;
}
```
- Remove console handle from `WaitForMultipleObjects()` array
- Remove `PeekNamedPipe()` polling logic (no longer needed)
- Console completions integrated with network I/O completions

**4. Main Thread Reading** (modify: `src/comm.c::get_user_data()`)
```c
#ifdef _WIN32
if (fd == STDIN_FILENO) {
    extern console_queue_t g_console_queue;
    size_t len;
    if (cq_dequeue(&g_console_queue, buf, text_space, &len)) {
        return len;  // Got complete line from worker
    }
    errno = EWOULDBLOCK;  // Queue empty
    return -1;
}
#endif
```

**5. Lifecycle Management** (modify: `src/backend.c`)
```c
void init_console_user() {
    extern io_reactor_t* g_reactor;
    
    // Determine console type
    console_type_t type = detect_console_type();  // REAL/PIPE/FILE
    
    // Initialize queue
    cq_init(&g_console_queue);
    
    // Start worker thread with IOCP handle
    console_worker_init(&g_console_worker, &g_console_queue, 
                       io_reactor_get_iocp(g_reactor), type);
}

void backend_shutdown() {
    // Signal worker to stop
    console_worker_shutdown(&g_console_worker);
    
    // Cleanup queue
    cq_destroy(&g_console_queue);
}
```

## Benefits

### 1. Solves Backend Timeout Issue (All stdin types)
**Before (PeekNamedPipe polling):**
```
Command arrives → waits up to 60s → reactor checks → processes
```

**After (IOCP completion):**
```
Command arrives → ReadFile completes → PostQueuedCompletionStatus → reactor wakes INSTANTLY
```

| Scenario | Polling Approach | Completion Notification |
|----------|------------------|------------------------|
| Data arrives after 5s | ⚠️ Waits full 60s | ✅ Woken at 5s |
| Data arrives after 30s | ⚠️ Waits full 60s | ✅ Woken at 30s |
| Rapid commands | Must wait for reactor cycle | ✅ Each completion wakes reactor |

### 2. Native Line Editing (CONSOLE_TYPE_REAL)
Users get full Windows console features:
- Backspace/Delete for character removal
- Left/Right arrow keys for cursor positioning
- Home/End for line navigation
- Up/Down arrows for command history (if console history enabled)
- F7 for command history popup
- Ctrl+C handling via console control handler

### 3. Non-Blocking Main Loop (All types)
- Worker thread handles blocking reads (`ReadConsole`/`ReadFile`)
- Main event loop never blocks on console input
- Network I/O, timers, and game logic continue unaffected
- Listening sockets accept connections regardless of console state

### 4. Unified Architecture
- Console input becomes just another IOCP completion source
- Same code path as network I/O (AcceptEx, WSARecv, WSASend)
- No special console handling in reactor wait loop
- Architecturally consistent with Windows I/O model

### 5. Zero Polling Overhead
- No `PeekNamedPipe()` calls every reactor cycle
- No console availability checking
- Worker blocks efficiently (zero CPU when idle)

### 6. Better User Experience
- **testbot.py**: Instant command execution (no 60s delay)
- **Interactive console**: Native line editing like cmd.exe
- **Redirected files**: Sequential reading without delays
- **UTF-8 Support**: `ReadConsole()` with `CP_UTF8` or UTF-16 conversion

## Challenges & Considerations

### Thread Safety
**Idea**: Check pipe availability with `PeekNamedPipe()` before `ReadFile()`  
**Implemented**: Currently in use, prevents blocking  
**Limitation**: Still subject to 60s backend timeout (polling only checks when timeout expires)  
**Status**: Worker thread with completion notification supersedes this approach

### 4. Worker Thread with Event Signaling
**Idea**: Worker calls `SetEvent()`, main thread adds event to `WaitForMultipleObjects()`  
**Limitation**: Requires modifying wait handle array, more complex than completion  
**Better**: Post completion directly to existing IOCP queue (cleaner integration)

### 5. Hybrid Mode Switching
**Idea**: Switch between raw and line mode based on user preference  
**Rejected**: Mode switching at runtime is complex; worker thread is cleaner

**Polling Approach (Solution B - current):**
```c
// Every reactor cycle (every 60s or when network I/O completes)
if (console_type == PIPE) {
    DWORD bytes_available = 0;
    PeekNamedPipe(stdin, NULL, 0, NULL, &bytes_available, NULL);
    if (bytes_available > 0) {
        signal_event_read();  // Can now read safely
    }
}
```
✅ Prevents blocking  
❌ Still subject to 60s timeout  
❌ Polling overhead on every reactor cycle  
⚠️ Only works for pipes, not real console

**Completion Approach (this design):**
```c
// Worker thread (dedicated, blocks harmlessly)
ReadFile(stdin, buf, size, &bytes_read, NULL);  // BLOCKS
PostQueuedCompletionStatus(iocp, bytes_read, CONSOLE_KEY, NULL);  // WAKES REACTOR

// Main thread (no changes needed - existing IOCP wait)
GetQueuedCompletionStatus(iocp, &bytes, &key, &overlapped, 60000);
if (key == CONSOLE_KEY) { /* Console data ready */ }
```
✅ Prevents blocking  
✅ **Instant wake** (no timeout wait)  
✅ Zero polling overhead  
✅ Works for **all console types** (real, pipe, file)  
✅ Architecturally consistent with IOCP model

### Thread Safety
- **Critical sections** protect queue operations (enqueue/dequeue)
- **Manual-reset event** signals main thread when data available
- **Shutdown synchronization**: Main thread must wait for worker to exit cleanly

### Buffer Management
- **Circular queue** prevents unbounded memory growth
- **Queue full policy**: Drop oldest lines or block worker? (Recommend: drop oldest)
- **Line size limits**: Enforce maximum line length (4K recommended)

### Console Mode Conflicts
- Worker needs `ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT`
- Cannot switch modes at runtime (worker owns console)
- **Trade-off**: Lose raw mode flexibility for native editing

### Shutdown Sequence
```c
void console_worker_shutdown(console_worker_t* worker) {
    worker->running = false;
    SetEvent(worker->shutdown_event);
    
    // Unblock ReadConsole by closing/reopening stdin (tricky)
    // OR: Use timeout-based read in worker with periodic shutdown checks
    
    WaitForSingleObject(worker->thread_handle, 5000);  // 5s timeout
    CloseHandle(worker->thread_handle);
}
```

**Problem**: `ReadConsole()` blocks indefinitely - cannot be interrupted by `shutdown_event`  
**Solution**: Use `WaitForMultipleObjects()` with overlapped I/O, or inject dummy input to unblock

### Error Handling
- Worker thread errors (console closed, read failure) must propagate to main thread
- Queue status tracking (overflow conditions)
- Thread creation failure fallback (revert to current raw mode?)

## Implementation Phases

### Phase 1: Core Infrastructure
- Implement `console_queue.c` with thread-safe circular buffer
- Add unit tests for queue operations (concurrent enqueue/dequeue)
- Document queue API and thread safety guarantees

### Phase 2: Worker Thread
- Implement `console_worker.c` with blocking `ReadConsole()` loop
- Add UTF-8 encoding support (code page or conversion)
- Handle shutdown signaling and thread join
- Unit tests with mock console input

### Phase 3: Integration
- Modify `io_reactor_win32.c` to use queue event instead of polling
- Update `get_user_data()` in `comm.c` to dequeue lines
- Update `backend.c` lifecycle (start worker, shutdown worker)
- Integration tests with actual console input

### Phase 4: Refinement
- Performance tuning (queue size, buffer sizes)
- Error recovery (worker crash handling)
- Console control handler for Ctrl+C (if not already handled)
- Documentation updates

## Testing Strategy

### Unit Tests
- `test_console_queue`: Thread safety, overflow handling, empty queue
- `test_console_worker`: Mock stdin, shutdown sequence, error conditions

### Integration Tests
- Manual console testing: Verify line editing works (backspace, arrows, etc.)
- Stress test: Rapid line input while network I/O active
- Shutdown test: Verify clean exit with pending console input

### Platform Compatibility
- Windows 10+ (primary target)
- Windows Server 2019+ (server deployments)
- Test with different code pages (UTF-8, Western European, etc.)

## Alternative Approaches Considered

### 1. Overlapped I/O for Console
**Idea**: Use overlapped `ReadFile()` with `OVERLAPPED` structure  
**Rejected**: Console handles don't support true overlapped I/O (no IOCP integration)

### 2. Polling with Timeout
**Idea**: Use `WaitForSingleObject(console, timeout)` with short timeout  
**Rejected**: Still blocks main loop during timeout, defeats non-blocking goal

### 3. Hybrid Mode Switching
**Idea**: Switch between raw and line mode based on user preference  
**Rejected**: Mode switching at runtime is complex; worker thread is cleaner

## Migration Path

This enhancement is **backward compatible**:
- Current polling implementation remains functional during development
- Worker thread can coexist with polling initially (test both paths)
- Once validated, remove polling code entirely
- If worker thread fails to start, fallback to polling (fail-safe)

## Future Enhancements

- **Command history persistence**: Save/restore history across sessions
- **Tab completion**: Worker thread hooks into mudlib completion API
- **Custom key bindings**: Configurable keymaps for console mode
- **Multi-line input**: Handle Ctrl+Enter for multi-line commands

## References

- [Windows Console I/O](https://learn.microsoft.com/en-us/windows/console/console-functions)
- [ReadConsole API](https://learn.microsoft.com/en-us/windows/console/readconsole)
- [Console Modes](https://learn.microsoft.com/en-us/windows/console/setconsolemode)
- [Thread Synchronization](https://learn.microsoft.com/en-us/windows/win32/sync/synchronization)

## Estimated Effort

- **Phase 1** (Queue): 2-3 days (implementation + tests)
- **Phase 2** (Worker): 3-4 days (thread management + UTF-8)
- **Phase 3** (Integration): 2-3 days (reactor + comm + backend)
- **Phase 4** (Refinement): 2-3 days (polish + docs)

**Total**: ~10-13 days for full implementation and testing

---

**Next Steps**: Review with community, prioritize against other features, allocate development resources.
