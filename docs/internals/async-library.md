# Async Library Design

**Status**: Design Phase  
**Created**: 2026-01-19  
**Platform Support**: Windows, Linux, macOS  
**Location**: `lib/async/`  
**Related**: [async-use-cases.md](../plan/async-use-cases.md)

---

## Executive Summary

The async library provides platform-agnostic infrastructure for offloading blocking operations to worker threads while maintaining Neolith's single-threaded backend execution model.

**Core Components**:
1. **async_queue** - Thread-safe FIFO message queue (MPSC/SPSC patterns)
2. **async_worker** - Managed worker threads with graceful shutdown
3. **async_runtime** - Event loop runtime for I/O and worker completions

**Internal Dependencies** (lib/port):
- **port_sync** - Platform-agnostic mutexes and events (internal use only)

**Validated Use Cases**:
- ✅ **Console Input** - Native line editing + testbot automation (HIGH PRIORITY)
- ✅ **DNS Resolution** - Non-blocking `getaddrinfo()` (PERFORMANCE CRITICAL)
- ✅ GUI clients, REST API, git operations, MCP server (Future)

**Design Principles**:
- Zero driver dependencies (fully reusable, testable in isolation)
- Platform-agnostic API with platform-specific implementations
- Single-threaded LPC execution preserved (workers never touch interpreter state)
- Event-driven main thread (never blocks on mutexes)

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│ lib/async/ (High-Level Async Runtime)                       │
│  ┌──────────────────┐  ┌─────────────────┐  ┌─────────────┐│
│  │  async_queue     │  │  async_worker   │  │async_runtime││
│  │ (message queue)  │  │(worker threads) │  │(event loop) ││
│  └──────────────────┘  └─────────────────┘  └─────────────┘│
└───────────────────────────────┬─────────────────────────────┘
                                │ uses
┌───────────────────────────────▼─────────────────────────────┐
│ lib/port/ (Low-Level Platform Primitives)                   │
│  - port_sync.{h,c} (mutexes, events) - INTERNAL USE ONLY    │
└──────────────────────────────────────────────────────────────┘
```

**Layering Principle**: High-level async abstractions (lib/async) built on low-level platform primitives (lib/port). Main thread code uses only lib/async APIs.

---

## Design Rationale

### Why async_runtime Instead of io_reactor?

**Problem**: `io_reactor` in `lib/port/` violated semantic layering—it's not a portability primitive, it's the async runtime.

**Industry Alignment**:
- Node.js: libuv (async runtime)
- Rust: tokio runtime
- Neolith: async_runtime

**Benefits**:
- ✅ Clear ownership: backend owns async runtime (not "portability layer")
- ✅ Workers call `async_runtime_post_completion()` directly (no wrapper)
- ✅ Single `async_runtime_wait()` returns all async events
- ✅ Proper layering: lib/async (high-level) → lib/port (low-level)

### Why No Explicit Synchronization in Main Thread?

**Event-Driven Architecture Principle**: Neolith's backend uses demultiplexed I/O. Main thread:
1. Waits for I/O events via `async_runtime_wait()`
2. Processes events without blocking
3. Returns to event loop

**Key Insight**: `port_sync` primitives (mutexes/events) are **encapsulated** within async_queue and async_worker implementations. Main thread never touches them directly.

**Why This Matters**:
- ✅ Prevents accidental blocking (main thread API has no blocking calls)
- ✅ Matches event loop model (demultiplex first, then process)
- ✅ Type safety: main thread can't access `port_mutex_t`, can't misuse it

---

## Component APIs

### 1. async_queue - Thread-Safe Message Queue

**Purpose**: Pass fixed-size messages between threads (typically worker → main).

**Key Types**:
```c
typedef struct async_queue_s async_queue_t;

typedef enum {
    ASYNC_QUEUE_DROP_OLDEST = 0x01,    // Drop oldest when full
    ASYNC_QUEUE_BLOCK_WRITER = 0x02,   // Block enqueue when full  
    ASYNC_QUEUE_SIGNAL_ON_DATA = 0x04  // Signal event on enqueue
} async_queue_flags_t;
```

**Core API**:
```c
async_queue_t* async_queue_create(size_t capacity, size_t max_msg_size, async_queue_flags_t flags);
void async_queue_destroy(async_queue_t* queue);

bool async_queue_enqueue(async_queue_t* queue, const void* data, size_t size);  // Non-blocking
bool async_queue_dequeue(async_queue_t* queue, void* buffer, size_t buffer_size, size_t* out_size);  // Non-blocking

bool async_queue_is_empty(const async_queue_t* queue);
bool async_queue_is_full(const async_queue_t* queue);
void async_queue_clear(async_queue_t* queue);
```

**Design Choices**:
- **Fixed-size messages**: Circular buffer with pre-allocated storage
- **Non-blocking dequeue**: Main thread never blocks waiting for data
- **Overflow policy**: Configurable (drop oldest or block enqueue)
- **Platform implementations**: Critical sections (Windows), pthread mutex (POSIX)

**Usage Pattern**:
1. Worker thread: `async_queue_enqueue()` + `async_runtime_post_completion()`
2. Main thread: Receives event from `async_runtime_wait()`, calls `async_queue_dequeue()` in loop

---

### 2. async_worker - Managed Worker Threads

**Purpose**: Abstraction for worker threads with graceful shutdown.

**Key Types**:
```c
typedef void* (*async_worker_proc_t)(void* context);
typedef struct async_worker_s async_worker_t;

typedef enum {
    ASYNC_WORKER_STOPPED,
    ASYNC_WORKER_RUNNING,
    ASYNC_WORKER_STOPPING
} async_worker_state_t;
```

**Core API**:
```c
async_worker_t* async_worker_create(async_worker_proc_t proc, void* context, size_t stack_size);
void async_worker_destroy(async_worker_t* worker);

void async_worker_signal_stop(async_worker_t* worker);  // Request shutdown
bool async_worker_join(async_worker_t* worker, int timeout_ms);  // Wait for exit

async_worker_t* async_worker_current(void);  // Get current worker handle
bool async_worker_should_stop(async_worker_t* worker);  // Check shutdown flag
```

**Design Choices**:
- **Platform abstraction**: CreateThread (Windows), pthread_create (POSIX)
- **Graceful shutdown**: Signal flag + join with timeout
- **Thread-local storage**: Worker can query its own handle
- **Stack size**: Configurable (0 = platform default)

**Usage Pattern**:
```c
void* worker_proc(void* ctx) {
    async_worker_t* self = async_worker_current();
    while (!async_worker_should_stop(self)) {
        // Do blocking work
        async_queue_enqueue(queue, result, size);
        async_runtime_post_completion(runtime, COMPLETION_KEY, data);
    }
    return NULL;
}
```

---

### 3. async_runtime - Event Loop Runtime

**Purpose**: Unified event loop for I/O events and worker completions (replaces lib/port/io_reactor).

**Key Types**:
```c
typedef struct async_runtime_s async_runtime_t;

typedef struct {
    int fd;                      // File descriptor (POSIX) or INVALID_FD
    HANDLE handle;               // Native handle (Windows)
    uintptr_t completion_key;    // User-defined key for completion correlation
    uint32_t event_type;         // EVENT_READ, EVENT_WRITE, EVENT_ERROR
    void* context;               // User context
} io_event_t;
```

**Core API**:
```c
async_runtime_t* async_runtime_init(void);
void async_runtime_deinit(async_runtime_t* runtime);

// I/O source management
int async_runtime_add(async_runtime_t* runtime, int fd, uint32_t events, void* context);
int async_runtime_modify(async_runtime_t* runtime, int fd, uint32_t events);
int async_runtime_remove(async_runtime_t* runtime, int fd);

// Event loop
int async_runtime_wait(async_runtime_t* runtime, io_event_t* events, int max_events, int timeout_ms);

// Worker completion posting (called from worker threads)
void async_runtime_post_completion(async_runtime_t* runtime, uintptr_t completion_key, uintptr_t data);
```

**Platform Implementations**:
- **Windows**: `async_runtime_iocp.c` - I/O Completion Ports
- **Linux**: `async_runtime_epoll.c` - epoll + eventfd for completion delivery
- **Fallback**: `async_runtime_poll.c` - poll + pipe for completion delivery

**Design Choices**:
- **Unified event loop**: Single `async_runtime_wait()` returns both I/O and worker completions
- **Worker notification**: `async_runtime_post_completion()` wakes main thread instantly
  - Windows: `PostQueuedCompletionStatus()` to IOCP
  - Linux: `write()` to eventfd
  - macOS/BSD: `write()` to pipe
- **Semantic correctness**: Runtime belongs in lib/async (async operations), not lib/port (platform primitives)

**Usage Pattern (Main Thread)**:
```c
async_runtime_t* runtime = async_runtime_init();

while (running) {
    io_event_t events[64];
    int n = async_runtime_wait(runtime, events, 64, timeout_ms);
    
    for (int i = 0; i < n; i++) {
        if (events[i].completion_key == CONSOLE_COMPLETION_KEY) {
            // Worker posted completion - dequeue messages
            while (async_queue_dequeue(console_queue, line, sizeof(line), &len)) {
                process_console_input(line, len);
            }
        } else {
            // I/O event - handle socket read/write
            handle_socket_io(&events[i]);
        }
    }
}
```

---

## Use Case Integration Patterns

### Pattern 1: Console Input (Phase 2)

**Components**: async_queue + async_worker + async_runtime

**Worker Thread**:
```c
while (!async_worker_should_stop(worker)) {
    ReadConsole(stdin, buf, sizeof(buf), &bytes_read, NULL);  // BLOCKS
    async_queue_enqueue(queue, buf, bytes_read);
    async_runtime_post_completion(runtime, CONSOLE_KEY, bytes_read);  // WAKE MAIN THREAD
}
```

**Main Thread**:
```c
if (events[i].completion_key == CONSOLE_KEY) {
    while (async_queue_dequeue(queue, line, sizeof(line), &len)) {
        process_console_input(line, len);  // Non-blocking
    }
}
```

**Benefit**: Console input becomes another completion source in event loop, eliminates 60s polling delay.

### Pattern 2: Async DNS (Phase 3)

**Components**: async_queue + async_worker + async_runtime

**Worker Thread**:
```c
while (!async_worker_should_stop(worker)) {
    dns_request_t req;
    if (async_queue_dequeue(request_queue, &req, sizeof(req), NULL)) {
        struct addrinfo* result = NULL;
        int error = getaddrinfo(req.hostname, NULL, &hints, &result);  // BLOCKS
        
        dns_response_t resp = {.request_id = req.id, .error = error, .result = result};
        async_queue_enqueue(response_queue, &resp, sizeof(resp));
        async_runtime_post_completion(runtime, DNS_KEY, req.socket_index);
    }
}
```

**Main Thread**:
```c
if (events[i].completion_key == DNS_KEY) {
    dns_response_t resp;
    while (async_queue_dequeue(response_queue, &resp, sizeof(resp), NULL)) {
        if (resp.error == 0) {
            socket_connect_with_addr(resp.socket_index, resp.result->ai_addr);
        } else {
            socket_dns_error(resp.socket_index, resp.error);
        }
    }
}
```

**Benefit**: DNS timeouts (5+ seconds) no longer freeze driver. Multiple concurrent lookups.

---

## Implementation Roadmap

### Phase 1: Core Primitives (1 week)

**Deliverables**:
1. `lib/async/async_queue.{h,c}` - Message queue implementation
2. `lib/async/async_worker.{h,c}` - Worker thread abstraction
3. `lib/port/port_sync_{win32,pthread}.c` - Mutex/event primitives
4. **Refactor `lib/port/io_reactor` → `lib/async/async_runtime`**
   - Move io_reactor_{iocp,epoll,poll}.c to lib/async/
   - Rename to async_runtime_{iocp,epoll,poll}.c
   - Add `async_runtime_post_completion()` API
5. Unit tests: queue thread safety, worker lifecycle, runtime event delivery
6. CMake integration: Add lib/async to build, update dependencies

**Timeline**: 5-7 days

**Validation**: All unit tests pass. async_runtime_wait() returns both I/O and completion events.

### Phase 2: Console Worker Integration (3-5 days)

**Deliverables**:
1. `lib/port/console_worker.c` using async library
2. Backend integration: `backend.c` event loop handles console completions
3. Platform-specific console detection (REAL/PIPE/FILE)
4. UTF-8 encoding support (Windows code page handling)
5. Integration tests: testbot.py instant command execution

**Timeline**: 3-5 days

**Validation**: Testbot commands execute immediately (< 10ms latency). Windows console supports native line editing.

### Phase 3: Async DNS (3-5 days)

**Deliverables**:
1. `lib/socket/async_dns.{h,c}` - DNS worker pool
2. Socket state machine: Add DNS_RESOLVING state
3. Modify `socket_connect()` to detect hostnames vs IP addresses
4. Backend integration: Process DNS completions in event loop
5. Configuration: `async_dns_workers` setting (default: 2)

**Timeline**: 3-5 days

**Validation**: DNS timeouts don't freeze driver. Stress test: 100+ concurrent lookups.

### Phase 4: Documentation & Testing (2-3 days)

**Deliverables**:
1. Update [async.md](../manual/async.md) with console worker and DNS patterns
2. Update [socket efun docs](../efuns/) with async DNS behavior
3. Configuration documentation in [neolith.conf](../../src/neolith.conf)
4. Integration testing: console + sockets + heartbeats concurrent

**Total Timeline**: 2-3 weeks for console worker + async DNS  
**Incremental Delivery**: Phase 1 → Phase 2 can be shipped independently (console worker alone provides value)

---

## Architectural Evolution: async_runtime

### From io_reactor to async_runtime

The async library originally planned a separate `async_notifier` component for worker completion delivery. After architectural analysis (see [async-reactor-api-unification-analysis.md](async-reactor-api-unification-analysis.md)), this was refactored into `async_runtime` for cleaner semantics:

**Original Design** (rejected):
- `lib/port/io_reactor` - I/O event demultiplexing
- `lib/async/async_notifier` - Worker completion wrapper
- Two separate APIs with shared integration point

**Current Design** (implemented):
- `lib/async/async_runtime` - Unified event loop for I/O **and** worker completions
- Single API, single event loop, clearer ownership

### Why async_runtime?

**Semantic Correctness**: The object that handles I/O events, worker completions, and timers is not a "portability primitive" (lib/port) - it's the **async runtime**. It belongs in lib/async as the foundational abstraction for asynchronous operations.

**Industry Alignment**: 
- Node.js: libuv (async runtime)
- Rust: tokio runtime
- Neolith: async_runtime

**Benefits**:
1. ✅ Workers call `async_runtime_post_completion()` directly (no wrapper)
2. ✅ Backend owns async runtime (not "portability layer")
3. ✅ Single `async_runtime_wait()` returns all async events
4. ✅ Proper layering: lib/async (high-level) → lib/port (low-level primitives)

### Validated Use Cases

All use cases analyzed in [async-use-cases.md](../plan/async-use-cases.md) and [async-dns-integration.md](../plan/async-dns-integration.md) confirmed compatible with async_runtime architecture:

1. **Console Input** (HIGH PRIORITY) - Windows console with native line editing + testbot stdin
2. **DNS Resolution** (PERFORMANCE CRITICAL) - Non-blocking `getaddrinfo()` for socket efuns
3. **GUI Clients** - Bidirectional channels for visual MUD clients
4. **REST API Calls** - HTTP client with request/response correlation
5. **Git Operations** - Background git commands with progress reporting
6. **MCP Server** - JSON-RPC over stdio for AI tool integration

See individual analysis documents for detailed patterns and implementation examples.

---

## Future Enhancements (Post-Phase 3)

### Optional Helper Abstractions

These patterns work with current API but could be packaged as convenience libraries:

1. **async_rpc.h** - Request/response correlation for REST/MCP
2. **async_channel.h** - Bidirectional communication wrapper (paired queues + workers)
3. **async_task.h** - Cancelable tasks with standardized progress reporting

See [async-use-cases.md](../plan/async-use-cases.md) for detailed evaluation. Current consensus: document patterns in user guide, implement helpers only if demand emerges.

### External Application Libraries

These use async library but are separate concerns:

1. **lib/http_client/** - HTTP/HTTPS client built on async workers
2. **lib/git_integration/** - Git operations with progress callbacks
3. **lib/mcp_server/** - MCP server implementation for AI tools

Implementation deferred pending Phase 1-3 completion and mudlib developer feedback.

### Performance Optimizations

**Lock-Free Queue (SPSC)**: For single-producer single-consumer scenarios, eliminate mutex overhead using C11 atomics. Complexity high, benefit marginal unless profiling shows contention.

**Thread Pool with Work Stealing**: Generalize worker pattern for better load balancing. Current multi-worker approach sufficient for validated use cases.

**Native Async File I/O**: Extend async_runtime to support platform async file APIs (Windows overlapped I/O, Linux io_uring, macOS kqueue). Complexity very high, defer until blocking file I/O becomes bottleneck.

---

## References

### Design Documents
- [async-dns-integration.md](../plan/async-dns-integration.md) - Async DNS implementation plan
- [async-use-cases.md](../plan/async-use-cases.md) - Extended use case validation
- [async-reactor-api-unification-analysis.md](async-reactor-api-unification-analysis.md) - Architectural rationale for async_runtime
- [console-async.md](../plan/console-async.md) - Console async integration plan

### User Documentation
- [async.md](../manual/async.md) - User guide with common patterns

### Related Infrastructure
- [io_reactor.h](../../lib/port/io_reactor.h) - Current location (to be moved to lib/async/async_runtime.h)
- [port_sync.h](../../lib/port/port_sync.h) - Internal synchronization primitives

### External References
- [libuv Design](https://docs.libuv.org/en/v1.x/design.html) - Similar async runtime architecture
- [Tokio Runtime](https://docs.rs/tokio/latest/tokio/runtime/) - Rust async runtime model
- [Windows IOCP](https://learn.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports) - Windows completion port documentation
- [Linux eventfd](https://man7.org/linux/man-pages/man2/eventfd.2.html) - Linux event notification mechanism
