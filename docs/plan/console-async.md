# Console Async Integration Plan

**Status**: Approved for Implementation  
**Priority**: HIGH (Phase 2 of async library roadmap)  
**Target Platform**: Windows (uses platform-agnostic async library)  
**Created**: 2026-01-02  
**Updated**: 2026-01-19 (async_runtime integration)  
**Related**: 
- [async-support.md](async-support.md) - Quick reference for async library design
- [async-library.md](../internals/async-library.md) - Complete async library technical design
- [async.md](../manual/async.md) - Async library user guide

---

## Critical Use Cases (Priority Order)

### 1. **Piped stdin for Testbot Automation** (HIGHEST PRIORITY)
**Current Problem**: 60-second polling delay makes automated testing impractical  
**Impact**: Blocks CI/CD integration, slows development workflow  
**Solution**: Worker thread with blocking `ReadFile()` + async_runtime notification  
**Benefit**: Commands execute immediately (< 10ms latency)

### 2. **Windows Console Mode with Native Editing** (HIGH PRIORITY)
**Current Problem**: Raw character mode loses Windows console features  
**Impact**: Poor UX for interactive console users (no backspace, arrows, F7 history)  
**Solution**: Worker thread with blocking `ReadConsole()` for line-based input  
**Benefit**: Full Windows console feature set

### 3. **File Redirection** (MEDIUM PRIORITY)
**Current Problem**: Same 60-second delay as piped stdin  
**Solution**: Same as piped stdin (blocking `ReadFile()`)  
**Benefit**: Log file replay for testing, debugging scenarios

---

## Problem Statement

Current Windows console implementation has two issues:

**Issue 1: Console Mode** (CONSOLE_TYPE_REAL)
- Uses `ReadConsoleInputW()` in raw mode (character-by-character)
- Mudlib provides command editing via LPC code
- **Missing**: Native Windows console features (backspace, arrow keys, command history via F7, etc.)

**Issue 2: Piped/File stdin** (CONSOLE_TYPE_PIPE, CONSOLE_TYPE_FILE)
- Current polling approach uses `PeekNamedPipe()` to check availability
- Still subject to backend 60-second timeout (reactor polls only when timeout expires)
- Commands can take up to 60 seconds to process even when data is ready

## Proposed Solution

Implement **worker thread with async_runtime completion notification** for all console input types using the platform-agnostic [async library](../internals/async-library.md):

```
┌──────────────────────────────────────────────────────────────┐
│ Main Thread (Backend Event Loop)                             │
│  ┌────────────────────────────────────────────────────┐     │
│  │ async_runtime_wait(runtime, events, ...)          │     │
│  │   - Network I/O completions (existing)            │     │
│  │   - Console worker completions (NEW!)      ◄──────┼─────┐│
│  │   - Timeout only when queue empty                 │     ││
│  └────────────────────────────────────────────────────┘     ││
│                     ▲                                        ││
│                     │ (async_runtime event loop)             ││
│                     │                                        ││
└─────────────────────┼────────────────────────────────────────┘│
                      │                                         │
┌─────────────────────┼─────────────────────────────────────────┘
│ Console Worker Thread (All stdin types)                      │
│  ┌────────────────────────────────────────────────────┐     │
│  │ while (!async_worker_should_stop(worker)) {        │     │
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
│  │     async_queue_enqueue(queue, buf, bytes_read);  │     │
│  │                                                     │     │
│  │     // POST COMPLETION - wakes main thread INSTANTLY!    │
│  │     async_runtime_post_completion(                 │     │
│  │       runtime,                                     │     │
│  │       CONSOLE_COMPLETION_KEY,                     │     │
│  │       bytes_read);  ────────────────────────────────────┘│
│  │   }                                                 │     │
│  │ }                                                   │     │
│  └─────────────────────────────────────────────────────┘     │
└───────────────────────────────────────────────────────────────┘
```

**Key Innovation:** Console input becomes just another **completion source** in the async runtime, alongside network I/O completions.

### Architecture Components

**1. Thread-Safe Line Queue** (uses: `lib/async/async_queue`)
```c
typedef struct {
    async_queue_t* queue;        // Generic async queue from lib/async
    size_t max_line_length;      // Maximum console line length (4KB)
} console_queue_t;

bool console_queue_init(console_queue_t* q, size_t capacity, size_t max_line_len);
bool console_queue_enqueue(console_queue_t* q, const char* line, size_t len);
bool console_queue_dequeue(console_queue_t* q, char* buf, size_t max_len, size_t* out_len);
void console_queue_destroy(console_queue_t* q);
```

**2. Console Worker Thread** (uses: `lib/async/async_worker`, new: `lib/port/console_worker.c`)
```c
typedef enum {
    CONSOLE_TYPE_REAL,   // Interactive console (ReadConsole)
    CONSOLE_TYPE_PIPE,   // Piped stdin (testbot.py)
    CONSOLE_TYPE_FILE    // File redirection
} console_type_t;

typedef struct {
    async_worker_t* worker;         // Generic worker from lib/async
    async_runtime_t* runtime;       // Runtime for completion posting
    console_queue_t* queue;
    console_type_t console_type;
} console_worker_t;

#define CONSOLE_COMPLETION_KEY ((uintptr_t)0xC0701E)  // Unique key

void* console_thread_proc(void* param) {
    console_worker_t* ctx = (console_worker_t*)param;
    async_worker_t* self = async_worker_current();
    
#ifdef _WIN32
    HANDLE stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
#else
    int stdin_fd = STDIN_FILENO;
#endif
    
    while (!async_worker_should_stop(self)) {
        char buf[4096];
        ssize_t bytes_read = 0;
        
#ifdef _WIN32
        DWORD bytes_read_win;
        BOOL success = FALSE;
        
        if (ctx->console_type == CONSOLE_TYPE_REAL) {
            success = ReadConsole(stdin_handle, buf, sizeof(buf) - 1, 
                                 &bytes_read_win, NULL);
        } else {
            success = ReadFile(stdin_handle, buf, sizeof(buf) - 1, 
                              &bytes_read_win, NULL);
        }
        
        if (!success) break;
        bytes_read = bytes_read_win;
#else
        bytes_read = read(stdin_fd, buf, sizeof(buf) - 1);
        if (bytes_read <= 0) break;
#endif
        
        if (bytes_read > 0) {
            buf[bytes_read] = '\0';
            console_queue_enqueue(ctx->queue, buf, bytes_read);
            
            // POST COMPLETION - wakes main thread via async_runtime
            // Windows: PostQueuedCompletionStatus to IOCP
            // POSIX: Write to eventfd or pipe
            async_runtime_post_completion(
                ctx->runtime,
                CONSOLE_COMPLETION_KEY,
                (uintptr_t)bytes_read
            );
        }
    }
    return NULL;
}

bool console_worker_init(console_worker_t* cworker, console_queue_t* queue, 
                        async_runtime_t* runtime, console_type_t type);
void console_worker_shutdown(console_worker_t* cworker);
```

**3. Backend Integration** (modify: `src/backend.c`)
```c
void backend_event_loop(void) {
    io_event_t events[64];
    
    while (running) {
        // Unified wait for I/O and worker completions
        int n = async_runtime_wait(runtime, events, 64, &timeout);
        
        for (int i = 0; i < n; i++) {
            if (events[i].completion_key == CONSOLE_COMPLETION_KEY) {
                // Console worker posted completion - data ready in queue!
                while (async_queue_dequeue(console_queue, line, sizeof(line), &len)) {
                    process_console_input(line, len);
                }
            } else {
                // Network I/O completion (existing code)
                handle_socket_io(&events[i]);
            }
        }
    }
}
```

**4. Lifecycle Management**
```c
void init_console_user() {
    // Determine console type (platform-specific detection)
    console_type_t type = detect_console_type();  // REAL/PIPE/FILE
    
    // Initialize queue (async library)
    console_queue_init(&g_console_queue, 256, 4096);
    
    // Start worker thread with async_runtime
    console_worker_init(&g_console_worker, &g_console_queue, 
                       g_runtime, type);
}

void backend_shutdown() {
    console_worker_shutdown(&g_console_worker);
    console_queue_destroy(&g_console_queue);
}
```

## Benefits

### 1. Instant Wake-up (All stdin types)
**Before:**
```
Command arrives → waits up to 60s → reactor checks → processes
```

**After:**
```
Command arrives → ReadFile completes → async_runtime_post_completion → reactor wakes INSTANTLY
```

### 2. Native Line Editing (CONSOLE_TYPE_REAL)
Users get full Windows console features:
- Backspace/Delete, Left/Right arrows, Home/End
- Up/Down arrows for command history
- F7 for command history popup
- Ctrl+C handling

### 3. Non-Blocking Main Loop
- Worker thread handles blocking reads
- Main event loop never blocks on console input
- Network I/O, timers, and game logic continue unaffected

### 4. Unified Architecture
- Console completions integrated with async_runtime event loop
- Same code path as network I/O
- No special console handling in reactor

## Implementation Phases

### Phase 1: Async Library Foundation (1 week)
- Implement `lib/async/async_queue.{h,c}` (platform-agnostic queue)
- Implement `lib/async/async_worker.{h,c}` (worker threads)
- Implement `lib/port/port_sync_{win32,pthread}.c` (mutexes, events)
- **Refactor `lib/port/io_reactor` → `lib/async/async_runtime`** (event loop runtime)
- Add `async_runtime_post_completion()` API
- Unit tests for async library components

### Phase 2: Console Worker (3-5 days)
- Implement `lib/port/console_worker.c` using async library
- Platform-specific console detection (REAL/PIPE/FILE)
- UTF-8 encoding support (Windows code page handling)
- Unit tests with mock stdin

### Phase 3: Integration (2-3 days)
- Update backend event loop to handle console completions
- Modify `get_user_data()` in `comm.c` to dequeue lines
- Update `backend.c` lifecycle (start/stop console worker)
- Integration tests with actual console input

### Phase 4: Testing & Refinement (2-3 days)
- Performance tuning (queue size, buffer sizes)
- Error recovery (worker crash handling)
- Cross-platform testing (Windows, Linux, macOS)
- Documentation updates

**Total**: ~2-3 weeks for full implementation

## Testing Strategy

### Unit Tests
- `test_console_queue`: Thread safety, overflow handling
- `test_console_worker`: Mock stdin, shutdown sequence

### Integration Tests
- Manual console testing: Verify line editing works
- Stress test: Rapid line input while network I/O active
- Testbot.py: Verify instant command execution (no delays)

## References

- [Async Library Design](../internals/async-library.md) - Core implementation
- [Async User Guide](../manual/async.md) - Usage patterns
- [Windows Console I/O](https://learn.microsoft.com/en-us/windows/console/console-functions)
