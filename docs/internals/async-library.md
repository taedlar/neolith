# Async Library Design (Developer Reference)

**Status**: Implemented (console worker + built-in DNS resolver integrated)  
**Audience**: Driver developers maintaining async infrastructure  
**Created**: 2026-01-19  
**Platform Support**: Windows, Linux, macOS  
**Location**: `lib/async/`  
**Related**: [addr-resolver.md](addr-resolver.md)

---

## Executive Summary

The async library provides platform-agnostic infrastructure for offloading blocking operations to worker threads while maintaining Neolith's single-threaded backend execution model.

**Core Components**:
1. **async_queue** - Thread-safe FIFO message queue (MPSC/SPSC patterns)
2. **async_worker** - Managed worker threads with graceful shutdown
3. **async_runtime** - Event loop runtime for I/O and worker completions

**Internal Dependencies** (lib/port):
- **port_sync** - Platform-agnostic mutexes and events (internal use only)

**Current Use Cases**:
- ✅ **Console Input** - Native line editing + testbot automation (HIGH PRIORITY)
- ✅ **DNS Resolution** - Non-blocking `getaddrinfo()` (PERFORMANCE CRITICAL)
- ✅ GUI clients, REST API, git operations, MCP server (supported by current primitives)

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

**CRITICAL CONSTRAINT: Single Wait Consumer**

`async_runtime_wait()` **MUST** be called from a single thread only (the main/backend thread). Violating this requirement causes undefined behavior:

❌ **NEVER DO THIS**:
- Call `async_runtime_wait()` from multiple threads simultaneously
- Call `async_runtime_wait()` again while a previous call is still blocked
- Call `async_runtime_wait()` from worker threads

**Why This Restriction Exists**:
1. **Event Correlation**: Completion keys and context pointers assume single consumer
2. **Platform Semantics**:
   - Windows IOCP: `GetQueuedCompletionStatus()` delivers each event to exactly one thread
   - Linux epoll: Edge-triggered events may be lost with multiple waiters
   - Event ordering guarantees break with concurrent consumers
3. **Driver Architecture**: Neolith's backend is single-threaded by design; all LPC execution happens on main thread

**Current Usage**: The driver calls `async_runtime_wait()` exclusively from `do_comm_polling()` in [src/comm.c](../../src/comm.c), which is invoked only from the main event loop in [src/backend.c](../../src/backend.c). This ensures the constraint is satisfied.

**Worker Threads**: Workers call `async_runtime_post_completion()` to **deliver** events to the main thread, but they never call `async_runtime_wait()` to **consume** events.

---

## Implementation Reference: Integration Patterns

The following patterns document how async primitives are integrated into driver subsystems. These serve as reference implementations for adding new async-driven features.

### Pattern 1: Console Input (Implemented)

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

**Benefit**: Console input becomes another completion source in the event loop with prompt command processing.

### Pattern 2: Built-In DNS Resolver Integration (Implemented)

**Components**: async_queue + async_worker + async_runtime

**Worker Side**:
```c
while (!async_worker_should_stop(worker)) {
    resolver_task_t task;
    if (async_queue_dequeue(request_queue, &task, sizeof(task), NULL)) {
        // Blocking resolver work happens on worker threads.
        resolver_result_t result = resolve_task(task);
        async_queue_enqueue(result_queue, &result, sizeof(result));
        async_runtime_post_completion(runtime, DNS_KEY, task.request_id);
    }
}
```

**Main Thread**:
```c
if (events[i].completion_key == DNS_KEY) {
    addr_resolver_result_t result;
    while (addr_resolver_dequeue_result(&result)) {
        if (result.status == ADDR_RESOLVER_OK) {
            socket_connect_with_addr(result.socket_index, &result.addr);
        } else {
            socket_dns_error(result.socket_index, result.error_code);
        }
    }
}
```

**Benefit**: DNS timeouts (5+ seconds) no longer freeze driver. Completions are correlated by request id and drained from a main-thread completion path.

---

## Implementation Status

### Completed Components

1. **lib/async/async_queue.{h,c}** - Bounded inter-thread message passing with configurable overflow policies
2. **lib/async/async_worker_*.c** - Platform-specific worker thread lifecycle (Windows/POSIX)
3. **lib/async/async_runtime_*.c** - Platform-specific unified I/O + completion event loop (IOCP/epoll/poll)
4. **lib/async/console_worker.{h,c}** - Console input worker integration
5. **src/addr_resolver.cpp** - Built-in DNS resolver with async worker backends (see [addr-resolver.md](addr-resolver.md))

### Critical Invariants for Developers

**Single-Consumer Constraint**:
- `async_runtime_wait()` is called exclusively from `do_comm_polling()` in [src/comm.c](../../src/comm.c)
- Called only from the backend's main event loop in [src/backend.c](../../src/backend.c)
- Violation causes undefined behavior on multi-waiter or concurrent-waiter scenarios

**Main-Thread Non-Blocking Guarantee**:
- Main thread never blocks on `async_queue_dequeue()` (always 0ms timeout)
- Main thread never calls `async_worker_join()` with indefinite timeout
- Synchronization primitives in [lib/port/sync.h](../../lib/port/sync.h) are internal to async library, never called by main thread

**Worker-to-Main Communication Pattern**:
- Workers enqueue results then call `async_runtime_post_completion()` (order matters for atomicity)
- Main thread drains result queues upon receiving completion key
- Completion keys are user-defined values for correlation (do not reuse across subsystems without coordination)

**LPC Interpreter Isolation**:
- Workers never touch LPC object state, interpreter state, or global symbol tables
- Result data must be self-contained (no pointers to transient allocations)
- Callbacks must check object validity (objects can be destroyed during async operations)

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

All use cases analyzed in this document and the shared resolver internals in [addr-resolver.md](addr-resolver.md) confirmed compatible with async_runtime architecture:

1. **Console Input** (HIGH PRIORITY) - Windows console with native line editing + testbot stdin
2. **DNS Resolution** (PERFORMANCE CRITICAL) - Non-blocking `getaddrinfo()` for socket efuns
3. **GUI Clients** - Bidirectional channels for visual MUD clients
4. **REST API Calls** - HTTP client with request/response correlation
5. **Git Operations** - Background git commands with progress reporting
6. **MCP Server** - JSON-RPC over stdio for AI tool integration

See individual analysis documents for detailed patterns and implementation examples.

---

## Potential Enhancements

### Optional Helper Abstractions

These patterns work with current API but could be packaged as convenience libraries:

1. **async_rpc.h** - Request/response correlation for REST/MCP
2. **async_channel.h** - Bidirectional communication wrapper (paired queues + workers)
3. **async_task.h** - Cancelable tasks with standardized progress reporting

See this document's use-case and future-enhancement sections for detailed evaluation. Current consensus: document patterns in user guide, implement helpers only if demand emerges.

### External Application Libraries

These use async library but are separate concerns:

1. **lib/http_client/** - HTTP/HTTPS client built on async workers
2. **lib/git_integration/** - Git operations with progress callbacks
3. **lib/mcp_server/** - MCP server implementation for AI tools

Implementation is optional and deferred until concrete demand emerges.

### Performance Optimizations

**Lock-Free Queue (SPSC)**: For single-producer single-consumer scenarios, eliminate mutex overhead using C11 atomics. Complexity high, benefit marginal unless profiling shows contention.

**Thread Pool with Work Stealing**: Generalize worker pattern for better load balancing. Current multi-worker approach sufficient for validated use cases.

**Native Async File I/O**: Extend async_runtime to support platform async file APIs (Windows overlapped I/O, Linux io_uring, macOS kqueue). Complexity very high, defer until blocking file I/O becomes bottleneck.

---

## References

### Design Documents
- [addr-resolver.md](addr-resolver.md) - Shared resolver module internals and DNS integration behavior
- [async-reactor-api-unification-analysis.md](async-reactor-api-unification-analysis.md) - Architectural rationale for async_runtime
- [async-phase2-console-worker-2026-01-20.md](../history/agent-reports/async-phase2-console-worker-2026-01-20.md) - Console worker implementation (Phase 2 complete)

### User Documentation
- [async.md](../manual/async.md) - User guide with common patterns

### Related Infrastructure
- [async_runtime.h](../../lib/async/async_runtime.h) - Unified async runtime API
- [sync.h](../../lib/port/sync.h) - Internal synchronization primitives

### External References
- [libuv Design](https://docs.libuv.org/en/v1.x/design.html) - Similar async runtime architecture
- [Tokio Runtime](https://docs.rs/tokio/latest/tokio/runtime/) - Rust async runtime model
- [Windows IOCP](https://learn.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports) - Windows completion port documentation
- [Linux eventfd](https://man7.org/linux/man-pages/man2/eventfd.2.html) - Linux event notification mechanism
