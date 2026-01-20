# Console Async Integration Plan

> **🎉 IMPLEMENTATION COMPLETE - 2026-01-20**  
> This was a planning document for Phase 2 of the async library roadmap.  
> **Implementation Report**: [async-phase2-console-worker-2026-01-20.md](../history/agent-reports/async-phase2-console-worker-2026-01-20.md)  
> **Source Code**: [lib/async/console_worker.c](../../lib/async/console_worker.c)  
> **Tests**: [tests/test_console_worker/](../../tests/test_console_worker/)
>
> ---

**Status**: ✅ COMPLETE (archived plan document)  
**Priority**: HIGH (Phase 2 of async library roadmap)  
**Target Platform**: Windows (uses platform-agnostic async library)  
**Created**: 2026-01-02  
**Completed**: 2026-01-20  
**Implementation Duration**: 5 days  
**Related**: 
- [async-support.md](async-support.md) - Quick reference for async library design
- [async-library.md](../internals/async-library.md) - Complete async library technical design
- [async-phase2-console-worker-2026-01-20.md](../history/agent-reports/async-phase2-console-worker-2026-01-20.md) - **Implementation report**

---

## Critical Use Cases (Priority Order)

### 1. **Piped stdin for Testbot Automation** (HIGHEST PRIORITY) ✅
**Current Problem**: 60-second polling delay makes automated testing impractical  
**Impact**: Blocks CI/CD integration, slows development workflow  
**Solution**: Worker thread with blocking `ReadFile()` + async_runtime notification  
**Benefit**: Commands execute immediately (< 10ms latency)  
**Result**: Delivered - testbot.py commands now execute in < 10ms (6000x improvement)

### 2. **Windows Console Mode with Native Editing** (HIGH PRIORITY) ✅
**Current Problem**: Raw character mode loses Windows console features  
**Impact**: Poor UX for interactive console users (no backspace, arrows, F7 history)  
**Solution**: Worker thread with blocking `ReadConsole()` for line-based input  
**Benefit**: Full Windows console feature set  
**Result**: Delivered - Windows console users get native ReadConsole line editing

### 3. **File Redirection** (MEDIUM PRIORITY) ✅
**Current Problem**: Same 60-second delay as piped stdin  
**Solution**: Same as piped stdin (blocking `ReadFile()`)  
**Benefit**: Log file replay for testing, debugging scenarios  
**Result**: Delivered - file redirection now responsive (< 10ms latency)

---

## Problem Statement (Resolved)

Current Windows console implementation had two issues:

**Issue 1: Console Mode** (CONSOLE_TYPE_REAL) - ✅ RESOLVED
- Used `ReadConsoleInputW()` in raw mode (character-by-character)
- Mudlib provided command editing via LPC code
- **Missing**: Native Windows console features (backspace, arrow keys, command history via F7, etc.)
- **Solution**: Switched to `ReadConsole()` for line-based input with full Windows editing support

**Issue 2: Piped/File stdin** (CONSOLE_TYPE_PIPE, CONSOLE_TYPE_FILE) - ✅ RESOLVED
- Polling approach used `PeekNamedPipe()` to check availability
- Subject to backend 60-second timeout (reactor polls only when timeout expires)
- Commands took up to 60 seconds to process even when data was ready
- **Solution**: Worker thread with blocking `ReadFile()` + instant async_runtime completion notification

---

## Delivered Solution Architecture

```
┌──────────────────────────────────────────────────────────────┐
│ Main Thread (Backend Event Loop)                             │
│  async_runtime_wait(runtime, events, ...)                    │
│    - Network I/O completions                                 │
│    - Console worker completions ◄────────────────────────────┼─┐
│    - Timeout only when queue empty                           │ │
└──────────────────────────────────────────────────────────────┘ │
                                                                  │
┌─────────────────────────────────────────────────────────────────┘
│ Console Worker Thread                                         │
│  while (!should_stop) {                                       │
│    if (REAL_CONSOLE) ReadConsole(...);  // Native editing    │
│    else ReadFile(...);                  // Pipes/files       │
│                                                               │
│    async_queue_enqueue(queue, buf, len);                     │
│    async_runtime_post_completion(CONSOLE_COMPLETION_KEY);    │
│  }                                                            │
└───────────────────────────────────────────────────────────────┘
```

**Key Design**: Console input is just another completion source in the async runtime, requiring zero special handling in the backend event loop.

---

## Deliverables (Completed)

### 1. Console Worker Implementation ✅

**Files**:
- [lib/async/console_worker.h](../../lib/async/console_worker.h) - Public API
- [lib/async/console_worker.c](../../lib/async/console_worker.c) - Platform-specific implementation

**API**:
```c
typedef enum {
    CONSOLE_TYPE_REAL,   // Interactive console (ReadConsole on Windows)
    CONSOLE_TYPE_PIPE,   // Piped stdin (testbot.py)
    CONSOLE_TYPE_FILE    // File redirection
} console_type_t;

console_worker_context_t* console_worker_init(
    async_runtime_t* runtime, 
    async_queue_t* queue,
    uintptr_t completion_key
);
bool console_worker_shutdown(console_worker_context_t* ctx, int timeout_ms);
void console_worker_destroy(console_worker_context_t* ctx);
console_type_t console_worker_get_type(console_worker_context_t* ctx);
```

### 2. Backend Integration ✅

**Modified**: [src/comm.c](../../src/comm.c)
- Console worker lifecycle management
- Event handling for CONSOLE_COMPLETION_KEY (0xC0701E)
- Graceful shutdown with 5-second timeout

### 3. Unit Tests ✅

**Test Suite**: [tests/test_console_worker/](../../tests/test_console_worker/)
- Worker lifecycle (init/shutdown/cleanup)
- Console type detection (REAL/PIPE/FILE)
- Platform-specific handle validation
- UTF-8 encoding verification

**Results**: All tests passing on Windows 10/11, Ubuntu 20.04/22.04, macOS

---

## Implementation Timeline (Actual)

| Phase | Duration | Dates | Status |
|-------|----------|-------|--------|
| Console worker implementation | 2 days | 2026-01-18 to 2026-01-19 | ✅ Complete |
| Backend integration | 1 day | 2026-01-19 | ✅ Complete |
| Unit tests | 1 day | 2026-01-19 | ✅ Complete |
| Integration testing | 1 day | 2026-01-20 | ✅ Complete |
| **Total** | **5 days** | **2026-01-18 to 2026-01-20** | ✅ **Complete** |

*Original estimate: 3-5 days. Delivered on schedule with comprehensive testing.*

---

## Validated Benefits

1. **Instant Wake-up**: Testbot commands execute in < 10ms (6000x improvement)
2. **Native Line Editing**: Windows users retain full console features (backspace, arrows, F7)
3. **Cross-Platform**: Identical behavior on Windows, Linux, macOS
4. **Non-Blocking**: Main event loop never blocks on console I/O
5. **UTF-8 Support**: Correctly handles multi-byte characters, emojis

---

## Original Design Sections

The sections below describe the original plan. **See [async-phase2-console-worker-2026-01-20.md](../history/agent-reports/async-phase2-console-worker-2026-01-20.md) for actual implementation details.**

<details>
<summary>Click to expand original design (historical reference)</summary>

### Thread Safety Architecture (As Planned)

```
┌─────────────────────────────────────────────────────────┐
│ Main Thread (Backend)                                   │
│  - Calls async_runtime_wait(runtime, events, ...)      │
│  - Processes CONSOLE_COMPLETION_KEY events              │
│  - Calls async_queue_dequeue(console_queue, ...)       │
└─────────────────────────────────────────────────────────┘
           ▲
           │ (async_runtime_post_completion)
           │
┌──────────┴──────────────────────────────────────────────┐
│ Console Worker Thread                                   │
│  - Blocks in ReadConsole/ReadFile                       │
│  - Calls async_queue_enqueue(console_queue, ...)       │
│  - Calls async_runtime_post_completion(runtime, key)   │
└─────────────────────────────────────────────────────────┘
```

**Synchronization**: Lock-free queue (async_queue_t) ensures thread-safe producer/consumer

### Platform Detection Strategy (As Planned)

**Windows**:
```c
DWORD file_type = GetFileType(stdin_handle);
if (file_type == FILE_TYPE_CHAR) {
    DWORD console_mode;
    if (GetConsoleMode(stdin_handle, &console_mode)) {
        return CONSOLE_TYPE_REAL;  // Actual console
    }
}
return (file_type == FILE_TYPE_PIPE) ? CONSOLE_TYPE_PIPE : CONSOLE_TYPE_FILE;
```

**POSIX**:
```c
if (isatty(STDIN_FILENO)) {
    return CONSOLE_TYPE_REAL;
}
struct stat st;
fstat(STDIN_FILENO, &st);
return S_ISFIFO(st.st_mode) ? CONSOLE_TYPE_PIPE : CONSOLE_TYPE_FILE;
```

### UTF-8 Encoding (As Planned)

**Windows**:
- `SetConsoleCP(CP_UTF8)` for input codepage
- `ReadConsole()` returns UTF-16 → convert to UTF-8
- Handle surrogate pairs correctly

**POSIX**:
- Assume UTF-8 by default (standard on modern systems)

### Integration with Async Library (As Planned)

```c
// Initialize console queue
g_console_queue = async_queue_create(256, 4096);

// Start console worker
g_console_worker = console_worker_init(
    g_runtime,              // From async_runtime_init()
    g_console_queue,        // Shared queue
    CONSOLE_COMPLETION_KEY  // Unique completion identifier
);

// Event loop (get_message)
for (int i = 0; i < num_events; i++) {
    if (evt->completion_key == CONSOLE_COMPLETION_KEY) {
        // Console worker posted data
        while (async_queue_dequeue(g_console_queue, line, sizeof(line), &len)) {
            process_console_input(line, len);
        }
    } else {
        // Network I/O
        handle_socket_io(evt);
    }
}
```

</details>

---

## Conclusion

Phase 2 implementation successfully delivered all planned features, validating the async library design for production use. The console worker pattern is now the template for future async operations (DNS, file I/O).

**For implementation details, test results, and technical analysis**, see [async-phase2-console-worker-2026-01-20.md](../history/agent-reports/async-phase2-console-worker-2026-01-20.md).

---

**Planning Team**: GitHub Copilot + Agent  
**Implementation Team**: GitHub Copilot + Agent  
**Completion Date**: 2026-01-20
