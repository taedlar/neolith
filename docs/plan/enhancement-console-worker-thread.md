# Enhancement Plan: Console Worker Thread for Native Line Editing

**Status**: Proposed  
**Target Platform**: Windows  
**Created**: 2026-01-02  
**Related**: [io-reactor-phase3-console-support.md](agent-reports/io-reactor-phase3-console-support.md), [windows-io.md](../manual/windows-io.md)

## Problem Statement

Current Windows console implementation (Phase 3) uses `ReadConsoleInputW()` in raw mode:
- Reads individual key events (character-by-character)
- Mudlib provides command editing via LPC code
- **Missing**: Native Windows console features (backspace, arrow keys, command history via F7, etc.)

While functional and non-blocking, this approach loses Windows console line editing capabilities that users expect from native console applications.

## Proposed Solution

Implement **worker thread** for console input handling:

```
┌─────────────────────────────────────────────────────────────┐
│ Main Thread (Backend Event Loop)                            │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ WaitForMultipleObjects(IOCP, console, sockets)      │   │
│  │  - Console handle signals when worker has data      │   │
│  │  - get_user_data() polls line queue (non-blocking)  │   │
│  └─────────────────────────────────────────────────────┘   │
│                          ▲                                   │
│                          │ (thread-safe queue)              │
│                          │                                   │
└──────────────────────────┼───────────────────────────────────┘
                           │
┌──────────────────────────┼───────────────────────────────────┐
│ Console Worker Thread    │                                   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ while (running) {                                    │   │
│  │   ReadConsole(stdin, buf, sizeof(buf), &read, NULL) │ ◄─── BLOCKS HERE
│  │   // Returns when user presses Enter               │   │
│  │   EnqueueLine(buf, read);                          │   │
│  │   SetEvent(console_event);  // Signal main thread  │   │
│  │ }                                                   │   │
│  └─────────────────────────────────────────────────────┘   │
│  Console Mode: ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT       │
└─────────────────────────────────────────────────────────────┘
```

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
typedef struct {
    HANDLE thread_handle;
    HANDLE shutdown_event;          // Signal worker to exit
    console_queue_t* queue;
    volatile bool running;
} console_worker_t;

DWORD WINAPI console_thread_proc(LPVOID param) {
    console_worker_t* worker = (console_worker_t*)param;
    HANDLE handles[] = {worker->shutdown_event, GetStdHandle(STD_INPUT_HANDLE)};
    
    while (worker->running) {
        char buf[4096];
        DWORD num_read = 0;
        
        // This blocks until Enter pressed - OK because we're in worker thread
        if (!ReadConsole(GetStdHandle(STD_INPUT_HANDLE), buf, sizeof(buf) - 1, 
                         &num_read, NULL)) {
            break;  // Console closed or error
        }
        
        if (num_read > 0) {
            buf[num_read] = '\0';
            cq_enqueue(worker->queue, buf, num_read);
            SetEvent(worker->queue->data_ready_event);
        }
    }
    return 0;
}

bool console_worker_init(console_worker_t* worker, console_queue_t* queue);
void console_worker_shutdown(console_worker_t* worker);
```

**3. Reactor Integration** (modify: `lib/port/io_reactor_win32.c`)
- Replace `GetNumberOfConsoleInputEvents()` polling with queue event check
- Console event now signals when worker thread has enqueued data
- Remove console availability check (worker handles blocking)

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
- `init_console_user()`: Start worker thread
- `backend_shutdown()`: Signal shutdown, join worker thread, cleanup queue

## Benefits

1. **Native Line Editing**: Users get full Windows console editing:
   - Backspace/Delete for character removal
   - Left/Right arrow keys for cursor positioning
   - Home/End for line navigation
   - Up/Down arrows for command history (if console history enabled)
   - F7 for command history popup
   - Ctrl+C handling via console control handler

2. **Non-Blocking Main Loop**: Worker thread handles blocking `ReadConsole()` calls
   - Main event loop never blocks on console input
   - Network I/O, timers, and game logic continue unaffected
   - Listening sockets accept connections regardless of console state

3. **Better User Experience**: Console mode feels like native Windows application

4. **UTF-8 Support**: `ReadConsole()` with `CP_UTF8` code page or UTF-16 → UTF-8 conversion

## Challenges & Considerations

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
- Current raw mode implementation remains functional
- Worker thread is **opt-in** via config option (e.g., `console_line_mode: true`)
- If worker thread fails to start, fallback to raw mode

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
