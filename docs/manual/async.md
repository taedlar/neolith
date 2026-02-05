# Async Library User Guide

**Library**: `lib/async`  
**Status**: Production Ready (Phase 1-2 complete)  
**Platform Support**: Windows, Linux, macOS  
**Implementation**: See [lib/async/](../../lib/async/)

## Overview

The async library provides thread-safe message passing and worker thread management for Neolith. It enables clean separation between blocking I/O operations (which run in worker threads) and the main event loop (which must never block).

**When to use async library**:
- ✅ Blocking I/O operations (console input, file reads, network requests)
- ✅ CPU-intensive tasks that would freeze the game
- ✅ Cross-thread communication with main event loop
- ❌ Simple callbacks (use function pointers)
- ❌ Short-lived computations (overhead > benefit)

## Quick Start

### Basic Worker Thread

**Pattern**: Create worker → Do blocking work → Signal stop → Join with timeout → Destroy

```c
// Worker proc checks async_worker_should_stop() in loop
void* worker_proc(void* ctx) {
    while (!async_worker_should_stop(async_worker_current())) {
        perform_blocking_io();  // OK in worker thread
    }
    return NULL;
}

// Lifecycle: create → signal_stop → join → destroy
async_worker_t* w = async_worker_create(worker_proc, ctx, 0);
async_worker_signal_stop(w);
async_worker_join(w, 5000);  // 5s timeout
async_worker_destroy(w);
```

**See**: [async_worker.h](../../lib/async/async_worker.h) for full API

### Message Queue (Worker → Main Thread)

**Pattern**: Worker enqueues → Main thread drains queue in loop

```c
// Create: capacity, max_msg_size, flags
async_queue_t* q = async_queue_create(256, 512, ASYNC_QUEUE_DROP_OLDEST);

// Worker: enqueue (non-blocking)
async_queue_enqueue(queue, data, data_len);

// Main thread: drain queue
while (async_queue_dequeue(queue, buf, sizeof(buf), &len)) {
    process_message(buf, len);
}
```

**Flags**: `DROP_OLDEST` (drop old when full), `BLOCK_WRITER` (block until space), `SIGNAL_ON_DATA` (event notification)  
**See**: [async_queue.h](../../lib/async/async_queue.h) for full API

## Components

### 1. Message Queue (async_queue)

**Purpose**: Thread-safe FIFO for passing fixed-size messages between threads (typically worker → main).

**When to use**:
- ✅ Worker needs to send results to main thread
- ✅ Multiple producers, single consumer (MPSC pattern)
- ✅ Fixed-size messages (use heap allocation for variable data)
- ❌ Simple callbacks (use function pointers instead)

**Key features**:
- **Non-blocking dequeue**: Main thread never waits
- **Overflow policies**: Drop oldest, block writer, or fail enqueue
- **Thread-safe**: Multiple producers supported
- **Performance**: Lock-free design, >100K msgs/sec

**API**: See [async_queue.h](../../lib/async/async_queue.h)  
**Implementation**: [async_queue.c](../../lib/async/async_queue.c)

### 2. Worker Threads (async_worker)

**Purpose**: Managed threads with graceful shutdown and platform abstraction.

**When to use**:
- ✅ Blocking I/O operations (console, DNS, file I/O)
- ✅ CPU-intensive tasks that would freeze main loop
- ✅ Need graceful shutdown with timeout
- ❌ Short-lived tasks (overhead > benefit)

**Key features**:
- **Graceful shutdown**: Signal flag + join with timeout
- **Platform abstraction**: CreateThread (Windows) / pthread_create (POSIX)
- **Thread-local access**: `async_worker_current()` returns handle
- **State tracking**: STOPPED / RUNNING / STOPPING

**Worker responsibilities**:
1. Check `async_worker_should_stop()` frequently (every iteration or ~100ms)
2. Cleanup resources before returning
3. Never block indefinitely without checking shutdown flag

**API**: See [async_worker.h](../../lib/async/async_worker.h)  
**Implementation**: [async_worker_win32.c](../../lib/async/async_worker_win32.c), [async_worker_pthread.c](../../lib/async/async_worker_pthread.c)

### 3. Async Runtime (Event Loop Integration)

**Purpose**: Unified event loop for I/O events (sockets) and worker completions.

**Architecture**: Workers post completions via `async_runtime_post_completion()` → Main thread receives via `async_runtime_wait()` alongside I/O events.

**Platform implementations**:
- **Windows**: IOCP for unified I/O + completions
- **Linux**: epoll for I/O + eventfd for completions
- **macOS/BSD**: poll for I/O + pipe for completions

**Integration pattern**:
```c
// Worker: post completion (wakes main thread)
async_runtime_post_completion(runtime, COMPLETION_KEY, data);

// Main: unified wait (I/O + worker events)
int n = async_runtime_wait(runtime, events, 64, &timeout);
for (int i = 0; i < n; i++) {
    if (events[i].completion_key == MY_KEY) {
        process_worker_completion();
    } else {
        handle_socket_io(&events[i]);
    }
}
```

**Why async_runtime instead of io_reactor?**  
Semantic correctness: Runtime manages async operations (lib/async), not platform primitives (lib/port). See [async-library.md](../internals/async-library.md#design-rationale) for full rationale.

**API**: See [async_runtime.h](../../lib/async/async_runtime.h)  
**Implementation**: [async_runtime_iocp.c](../../lib/async/async_runtime_iocp.c), [async_runtime_epoll.c](../../lib/async/async_runtime_epoll.c), [async_runtime_poll.c](../../lib/async/async_runtime_poll.c)

**Note**: Synchronization primitives ([sync.h](../../lib/port/sync.h)) are internal to async library. User code should use async_queue/async_worker APIs, never call mutexes/events directly.

## Common Patterns

**Production implementations**: See actual code for complete details.

### Pattern 1: Console Input Worker

**Problem**: Console input blocks main event loop  
**Solution**: Worker thread reads stdin, posts completions to runtime

**Implementation**: [console_worker.c](../../lib/async/console_worker.c)  
**Integration**: [comm.c](../../src/comm.c) handles `CONSOLE_COMPLETION_KEY` events  
**Implementation**: [async-phase2-console-worker-2026-01-20.md](../history/agent-reports/async-phase2-console-worker-2026-01-20.md)

**Key insight**: Console becomes just another completion source in event loop, eliminating 60s polling delay.

### Pattern 2: Async DNS Resolution

**Problem**: Blocking `getaddrinfo()` freezes driver (5+ seconds on timeout)  
**Solution**: Worker pool handles DNS queries, posts results to main thread

**Status**: Design complete, ready for implementation (Phase 3)  
**Design**: [async-dns-integration.md](../plan/async-dns-integration.md)

**Key insight**: Multiple concurrent DNS lookups without blocking main loop.

### Pattern 3: Bidirectional Channel (GUI, MCP Server)

**Pattern**: Two queues (input/output) + reader/writer workers

**Use cases**:
- GUI client connections (VSCode extension, web admin panel)
- MCP server stdio (AI tool integration)
- SSH-like remote access

**Key components**:
- Input queue: External → Driver
- Output queue: Driver → External
- Reader worker: Blocking read + enqueue + post completion
- Writer worker: Dequeue + blocking write

**Example structure**: See Pattern 3 in [async.md](async.md#pattern-3-bidirectional-channel) (previous version) if needed, but prefer implementing based on console_worker pattern.

### Pattern 4: Request/Response Correlation

**Pattern**: Correlation ID links requests to responses in async operations

**Use cases**:
- REST API calls (HTTP client)
- JSON-RPC (MCP server)
- Distributed operations

**Key design**:
1. Assign unique ID to each request
2. Track pending requests in table
3. Worker includes ID in response
4. Main thread matches response to callback

**Implementation tip**: Use `uint64_t` for IDs, hash table for pending requests.

### Pattern 5: Progress Reporting

**Pattern**: Worker posts periodic progress updates during long operations

**Use cases**:
- Git clone/pull with progress bars
- File compression
- Database imports
- LPC recompilation

**Key design**:
- Progress message type with task_id + percent + status text
- Worker posts updates at checkpoints
- Main thread displays progress or calls LPC callback
- Final message includes completion status + exit code

### Pattern 6: Thread Pool

**Pattern**: Multiple workers dequeue from shared task queue

**Use cases**:
- CPU-intensive parallel processing
- Batch operations across multiple cores

**Key design**:
- Single task queue shared by N workers
- Workers compete for tasks (queue handles synchronization)
- Optional per-worker result queues or shared result queue
- Balance worker count with CPU cores (typically NUM_CPUS or NUM_CPUS-1)

## Best Practices

### 1. Queue Sizing
**Rule**: Capacity should handle ~1 second of peak message rate

```c
// Good: Sized for load (100 msgs/sec * 1s)
async_queue_t* q = async_queue_create(128, 1024, ASYNC_QUEUE_DROP_OLDEST);
```

### 2. Worker Shutdown
**Always** provide timeout to `async_worker_join()`. Workers must check `async_worker_should_stop()` frequently (every iteration or ~100ms max).

### 3. Thread Safety
Queues are thread-safe, but **data passed through them is not**. Always copy data into queue or transfer heap ownership clearly.

### 4. Error Handling
- Check all `create()` calls for NULL
- Decide graceful degradation policy for queue full
- Log errors for debugging

### 5. Performance
- **Batch dequeues**: Drain queue in loop, not one message per iteration
- **Fixed-size messages**: Use structs, pass pointers for variable data
- **Lock-free design**: Async queue uses lock-free MPSC implementation

See [async_queue.c](../../lib/async/async_queue.c) and [async_worker.c](../../lib/async/async_worker_win32.c) for implementation details.

## Debugging

Use `async_queue_get_stats()` to monitor queue health. Check for dropped messages (queue too small or consumer too slow).

**Memory leak detection**:
- Linux: `valgrind --leak-check=full ./neolith -f config.conf`
- Windows: `drmemory -- neolith.exe -f config.conf`

**Common deadlock causes**:
1. Worker blocked on `BLOCK_WRITER` queue
2. Main thread waiting for join, but worker blocked on queue
3. Solution: Use timeouts, avoid circular dependencies

## See Also

### Documentation
- **Plans and Use Cases**: [async-support.md](../plan/async-support.md) - Design roadmap and planned use cases
- **Design**: [async-library.md](../internals/async-library.md) - Architecture and platform implementations
- **Integration**: [async-phase2-console-worker-2026-01-20.md](../history/agent-reports/async-phase2-console-worker-2026-01-20.md) - Console worker implementation

### Implementation
- **Source**: [lib/async/](../../lib/async/) - All async library components
- **Tests**: [tests/test_async_queue/](../../tests/test_async_queue/), [tests/test_async_worker/](../../tests/test_async_worker/), [tests/test_console_worker/](../../tests/test_console_worker/)
- **Integration**: [src/comm.c](../../src/comm.c) - Console worker integration example

---

**Status**: Phase 1-2 production-ready. Phase 3 (async DNS) ready for implementation.  
**Questions?** See [CONTRIBUTING.md](../CONTRIBUTING.md)
