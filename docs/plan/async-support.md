# Async Library & Use Cases Summary

**Date**: 2026-01-19  
**Quick Reference**: Key findings from async library design and use case analysis

---

## What is the Async Library?

Platform-agnostic infrastructure for running blocking operations in worker threads without freezing Neolith's single-threaded backend. Provides thread-safe message queues, worker thread management, and event loop integration.

**Location**: `lib/async/`  
**Dependencies**: None (zero driver dependencies)  
**Platform Support**: Windows (WinAPI), Linux/macOS (pthread)

---

## Core Components

| Component | Purpose | Platform Impl |
|-----------|---------|---------------|
| **async_queue** | Thread-safe FIFO message queue | Platform-agnostic (mutex-protected circular buffer) |
| **async_worker** | Managed worker threads | `CreateThread()` / `pthread_create()` |
| **async_runtime** | Event loop runtime (I/O + completions) | IOCP / epoll / poll (moved from lib/port/io_reactor) |
| **port_sync** (lib/port) | Mutex/event primitives (internal) | `CRITICAL_SECTION` + `Event` / `pthread_mutex` + `pthread_cond` |

**Thread Safety Model**:
- Workers execute blocking I/O in background threads
- Results queued via thread-safe async_queue
- Main thread processes results in backend event loop (single-threaded LPC execution preserved)

---

## Validated Use Cases

### 1. **Console Input** (HIGH PRIORITY - Windows)

**Problem**: 
- Console mode lacks native line editing (backspace, arrows, F7 history)
- Piped stdin (testbot.py) suffers 60-second polling delay

**Solution**:
```
Worker Thread                  Main Thread (Backend)
─────────────────              ──────────────────────
ReadConsole() ──┌             ┌─ async_runtime_wait()
(blocks)        │             │    receives CONSOLE_COMPLETION_KEY
                │             │
Got line ───────├─ Enqueue ──→├─ Dequeue → process_console_input()
                └─ post_completion ─→┘
```

**Benefits**:
- ✅ Native Windows console features work
- ✅ Testbot commands execute immediately (no 60s delay)
- ✅ Platform-agnostic design (POSIX version uses eventfd/pipe)

**Implementation**: [console-async.md](console-async.md)

---

### 2. **DNS Resolution** (PERFORMANCE CRITICAL - Socket Efuns)

**Problem**: Blocking `gethostbyaddr()` freezes entire driver during DNS timeouts (5+ seconds)

**Solution**:
```
socket_connect("example.com 80", ...)
  ↓
Detect hostname → Submit dns_request_t to worker queue
  ↓ (worker thread)
getaddrinfo("example.com", ...) [BLOCKS in background]
  ↓
Post completion → Main thread processes → call connect()
```

**Benefits**:
- ✅ Driver never blocks on DNS lookups
- ✅ Multiple DNS queries processed in parallel (2-4 workers)
- ✅ Zero semantic changes (callbacks still single-threaded)

**Implementation**: [socket-efuns-async-analysis.md](socket-efuns-async-analysis.md)

---

### 3. **GUI Console Extension** (FUTURE)

**Pattern**: Bidirectional channels for GUI ↔ Driver communication

**Design**:
```c
typedef struct {
    async_queue_t* from_gui;   // GUI → Driver
    async_queue_t* to_gui;     // Driver → GUI
    async_worker_t* reader;    // Worker reads from GUI socket
    async_worker_t* writer;    // Worker writes to GUI socket
} gui_client_t;
```

**Use Cases**: Visual MUD clients, VSCode extensions, web-based admin panels

**Status**: API validated, implementation on-demand

---

### 4. **REST API Calls** (FUTURE)

**Pattern**: Request/response correlation with blocking HTTP in worker threads

**Design**:
```c
// LPC: mudlib_http_get("https://api.example.com/data", callback)
Submit http_request_t → Worker executes curl → Post completion
  → Main thread calls LPC callback with response
```

**Use Cases**: Webhooks, OAuth, external API integration (weather, Discord bots, etc.)

**Status**: API validated, implementation on-demand

---

### 5. **Git Operations** (FUTURE)

**Pattern**: Progress reporting with cancellation support

**Design**:
```c
Worker executes: git clone --progress <url>
  ↓ (periodic)
Parse progress → Enqueue progress_update_t
  ↓
Main thread calls LPC callback with progress (e.g., "Receiving objects: 45%")
```

**Use Cases**: In-game mudlib deployment, automated updates, developer tools

**Status**: API validated, implementation on-demand

---

### 6. **Model Context Protocol (MCP) Server** (FUTURE)

**Pattern**: JSON-RPC over stdio/sockets with request multiplexing

**Design**:
```c
Bidirectional workers:
  Reader: Parse JSON-RPC → Enqueue request
  Writer: Dequeue response → Send JSON
  
Main thread: Route requests to LPC handlers, queue responses
```

**Use Cases**: AI assistant integration (Claude, GPT), IDE extensions

**Status**: API validated, implementation on-demand

---

## Design Validation Results

✅ **All 6 use cases supported without API modifications**  
✅ **Current primitives sufficient** (queue, worker, sync, notifier)  
✅ **Optional helpers** can be added later (async_rpc, async_channel, async_task)

**Conclusion**: Core async library design is **complete and validated**. No changes needed for known use cases.

See [async-library-use-case-analysis.md](async-library-use-case-analysis.md) for detailed evaluation.

---

## Implementation Roadmap

### Phase 1: Core Primitives (1 week) ✅ COMPLETE
**Deliverables**:
- `lib/port/port_sync.{h,c}` - Platform-agnostic sync primitives ✅
- `lib/async/async_queue.{h,c}` - Uses port_sync internally ✅
- `lib/async/async_worker_{win32,pthread}.c` ✅
- `lib/async/async_runtime_{iocp,epoll,poll}.c` - Unified event loop ✅
- Unit tests (GoogleTest) ✅

**Success Criteria**:
- Queue throughput >10K msgs/sec ✅
- Worker creation <5ms ✅
- Zero memory leaks ✅

### Phase 2: Console Worker (3-5 days) ✅ COMPLETE — **IMMEDIATE VALUE DELIVERED**
**Deliverables**:
- `lib/async/console_worker.{h,c}` using async library ✅
- Platform-specific console detection (REAL/PIPE/FILE) ✅
- Backend integration: `src/comm.c` event loop handles console completions ✅
- UTF-8 encoding support (Windows code page handling) ✅
- Unit tests: `tests/test_console_worker/` ✅

**Target Benefits**:
- ✅ Windows console mode with native features
- ✅ Testbot automation without 60s delay (instant command execution)
- ✅ Cross-platform design (Windows and POSIX)

**Implementation Date**: 2026-01-20

### Phase 3: Async DNS (3-5 days) — **PERFORMANCE FIX**
**Deliverables**:
- `lib/socket/async_dns.{h,c}`
- New socket state: `DNS_RESOLVING`
- Backend integration

**Target Benefits**:
- ✅ No more driver freezes on DNS timeouts
- ✅ Parallel DNS resolution (2-4 workers)

### Phase 4: Documentation (1-2 days)
**Deliverables**:
- User guide updates
- Configuration documentation
- Integration testing

**Total Timeline**: 2-3 weeks for console + DNS async

---

## Integration with Existing Code

### I/O Reactor
**Relationship**: Complementary, not overlapping
- **io_reactor**: Manages non-blocking file descriptor events (sockets, pipes)
- **async_notifier**: Integrates worker completions into reactor event loop

**Integration Point**:
```c
// Expose IOCP/eventfd for notifier
HANDLE io_reactor_get_iocp(io_reactor_t* reactor);           // Windows
int io_reactor_get_event_loop_handle(io_reactor_t* reactor); // POSIX
```

### Backend Event Loop
**Current**:
```c
io_reactor_wait(reactor, events, 64, 60000);  // 60s timeout
for (each event) { dispatch socket handlers }
```

**With Async**:
```c
io_reactor_wait(reactor, events, 64, 60000);
for (each event) {
    if (event.key == CONSOLE_COMPLETION_KEY) {
        while (dequeue console_queue) { process_input() }
    } else if (event.key == DNS_COMPLETION_KEY) {
        process_dns_completions();
    } else {
        dispatch_socket_handler();
    }
}
```

### Mudlib Impact
**Zero changes required**. Async operations invisible to LPC code:
- Console input still appears synchronous
- Socket connects still trigger callbacks
- All applies execute in main thread (deterministic order)

---

## Future Enhancements (Not Required Now)

| Enhancement | Priority | Effort | Verdict |
|-------------|----------|--------|---------|
| **async_rpc** (request/response helper) | Medium | 2-3 days | Implement on-demand |
| **async_channel** (bidirectional helper) | Medium | 2-3 days | Implement on-demand |
| **Lock-free SPSC queue** | Low | 1-2 weeks | Only if profiling shows need |
| **Thread pool** | Low | 1-2 weeks | Current pattern sufficient |
| **Native async file I/O** (io_uring) | Research | 4+ weeks | Deferred indefinitely |
| **Coroutine support** (C23) | Research | Months | Blocked on language adoption |

**Recommendation**: Ship core primitives first. Add helpers reactively when use cases emerge.

---

## Key Documentation

| Document | Purpose |
|----------|---------|
| [async-library.md](../internals/async-library.md) | Complete technical design (API, structures, platform impl) |
| [async-use-cases.md](async-use-cases.md) | Extended use case validation |
| [async-dns-integration.md](async-dns-integration.md) | DNS async integration plan |
| [async.md](../manual/async.md) | User guide (usage patterns, examples) |
| [console-async.md](console-async.md) | Console async integration plan |

---

## Questions & Answers

**Q: Why not use existing libraries (libuv, Boost.Asio)?**  
A: External dependencies conflict with Neolith's zero-dependency policy. Platform-specific implementations are lightweight and tailored to driver needs.

**Q: Will this make the driver multi-threaded?**  
A: Worker threads only execute blocking I/O. All LPC execution remains single-threaded in the backend. No race conditions in mudlib code.

**Q: What about performance overhead?**  
A: Minimal. Workers block on I/O (zero CPU when idle). Queue operations are O(1). Notifier latency <10μs on Windows/Linux.

**Q: Can this break existing mudlibs?**  
A: No. All async operations are transparent to LPC. Callbacks execute in the same deterministic order as before.

**Q: When will this be implemented?**  
A: Phase 1 (core primitives) ready for implementation now. Phase 2 (console worker) targets Windows console improvement. Phase 3 (async DNS) depends on Phases 1-2.

---

**Next Steps**: Review design, approve implementation plan, begin Phase 1 (port_sync, async_queue, async_worker).
