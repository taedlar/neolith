# Async Library & IO Reactor API Unification Analysis

**Date**: 2026-01-19  
**Status**: COMPLETED - Migration finished 2026-01-20  
**Question**: With non-blocking async library design, should async_notifier and io_reactor share a common API?

---

## Migration Summary (2026-01-20)

**COMPLETED**: This analysis led to full migration from `io_reactor` to unified `async_runtime` API.

### What Was Removed
- `lib/port/io_reactor.h`, `io_reactor_win32.c`, `io_reactor_poll.c` (replaced by `async_runtime`)
- `tests/test_io_reactor/` (37 unit tests for deprecated API)
- Build targets for `io_reactor_*.c` files

### What Was Renamed
- `g_io_reactor` → `g_runtime` in `src/comm.c`
- `get_io_reactor()` → `get_async_runtime()` for clarity

### Final Architecture
- Production code uses `async_runtime_init()`, `async_runtime_add()`, `async_runtime_wait()`
- Unified event loop handles both I/O and worker completions
- Platform implementations: `async_runtime_iocp.c` (Windows), `async_runtime_epoll.c` (Linux), `async_runtime_poll.c` (fallback)

See [async-library.md](../../internals/async-library.md) for current async library design.

---

## Original Analysis (Historical)

## Current Architecture

### IO Reactor API (lib/port/io_reactor.h)

**Purpose**: Demultiplex I/O readiness events (file descriptors)

**Key Operations**:
```c
// Registration (non-blocking)
int io_reactor_add(io_reactor_t* reactor, socket_fd_t fd, void* context, int events);
int io_reactor_modify(io_reactor_t* reactor, socket_fd_t fd, int events);
int io_reactor_remove(io_reactor_t* reactor, socket_fd_t fd);

// Event loop (BLOCKS waiting for I/O)
int io_reactor_wait(io_reactor_t* reactor, io_event_t* events, int max_events, 
                    struct timeval* timeout);

// Interrupt wait (non-blocking, thread-safe)
int io_reactor_wakeup(io_reactor_t* reactor);
```

**Event Structure**:
```c
typedef struct io_event_s {
    void* context;           // User context (e.g., socket object pointer)
    int event_type;          // EVENT_READ | EVENT_WRITE | EVENT_ERROR
    uintptr_t completion_key; // IOCP completion key (Windows)
    int bytes_transferred;   // IOCP bytes (Windows)
    void* buffer;            // IOCP buffer (Windows)
} io_event_t;
```

### Async Notifier API (lib/async/async_notifier.h)

**Purpose**: Notify main thread when worker completes task

**Key Operations**:
```c
// Creation (integrates with reactor)
async_notifier_t* async_notifier_create(void* event_loop_handle);
                                         // Windows: HANDLE iocp
                                         // POSIX: io_reactor_t*

// Post notification (non-blocking, thread-safe)
bool async_notifier_post(async_notifier_t* notifier, 
                         uintptr_t completion_key, 
                         uintptr_t data);

// Poll notifications (non-blocking)
bool async_notifier_poll(async_notifier_t* notifier, 
                         uintptr_t* out_key, 
                         uintptr_t* out_data);

void async_notifier_destroy(async_notifier_t* notifier);
```

**No explicit event structure** - notifications delivered via io_reactor events.

---

## Current Integration Model

### Windows (IOCP)

```c
// Setup
io_reactor_t* reactor = io_reactor_create();
async_notifier_t* notifier = async_notifier_create(io_reactor_get_iocp(reactor));

// Worker thread
async_notifier_post(notifier, WORKER_KEY, result_code);
  ↓ (calls PostQueuedCompletionStatus internally)

// Main thread - UNIFIED event loop
io_event_t events[64];
int n = io_reactor_wait(reactor, events, 64, &timeout);

for (int i = 0; i < n; i++) {
    if (events[i].completion_key == WORKER_KEY) {
        // Worker completion (from async_notifier)
        process_worker_result();
    } else {
        // Socket I/O (from io_reactor_add)
        process_socket_event(&events[i]);
    }
}
```

**Key Insight**: Both event types already flow through **same wait call** (`io_reactor_wait`). They are **already unified at consumption**.

### POSIX (poll/epoll + eventfd)

```c
// Setup
io_reactor_t* reactor = io_reactor_create();
async_notifier_t* notifier = async_notifier_create(reactor);
  ↓ (creates eventfd, calls io_reactor_add internally)

// Worker thread
async_notifier_post(notifier, WORKER_KEY, data);
  ↓ (writes to eventfd)

// Main thread - UNIFIED event loop
io_event_t events[64];
int n = io_reactor_wait(reactor, events, 64, &timeout);

for (int i = 0; i < n; i++) {
    if (events[i].context == notifier_eventfd_context) {
        // Worker completion - must poll to get key/data
        uintptr_t key, data;
        async_notifier_poll(notifier, &key, &data);
        process_worker_result(key, data);
    } else {
        // Socket I/O
        process_socket_event(&events[i]);
    }
}
```

**Key Insight**: `async_notifier` is a **thin wrapper** that:
1. Creates eventfd/pipe
2. Registers it with io_reactor
3. Provides typed posting/polling interface

---

## API Comparison Matrix

| Aspect | io_reactor | async_notifier | Overlap? |
|--------|-----------|----------------|----------|
| **Purpose** | Demux I/O readiness | Demux worker completions | Both demux events |
| **Event sources** | File descriptors | Worker threads | Different domains |
| **Registration** | `io_reactor_add(fd, ...)` | `async_notifier_create(reactor)` | Different semantics |
| **Event posting** | Kernel (I/O ready) | `async_notifier_post()` | Different triggers |
| **Event consumption** | `io_reactor_wait()` | `io_reactor_wait()` | ✅ **SAME** |
| **Platform abstraction** | IOCP / poll / epoll | IOCP / eventfd / pipe | Both abstract |
| **Blocking behavior** | `wait()` blocks | All non-blocking | Different |
| **Thread safety** | Not thread-safe (main thread only) | `post()` thread-safe | Different requirements |

---

## Option 1: Keep Separate (Current Design) ✅ RECOMMENDED

**Rationale**: Different semantic domains with clean integration point.

### Pros:
1. **Clear separation of concerns**
   - io_reactor: I/O readiness (kernel-driven)
   - async_notifier: Worker completions (application-driven)

2. **Different usage patterns**
   - File descriptors: Register once, get multiple events
   - Worker completions: One-shot notifications

3. **Type safety**
   - `io_reactor_add()` takes `socket_fd_t` (compile-time check)
   - `async_notifier_post()` takes `uintptr_t` (opaque data)

4. **Platform differences**
   - Windows: Both use IOCP (natural unification already exists)
   - POSIX: async_notifier wraps eventfd creation + reactor registration

5. **Integration already optimal**
   - Single `io_reactor_wait()` call returns both event types
   - No performance overhead
   - Clean discrimination via `completion_key` or `context` pointer

### Cons:
- Slightly more API surface (but only 4 functions in async_notifier)
- Conceptual overhead (users must learn two APIs)

---

## Option 2: Unified Event Source API

**Proposal**: Abstract both into generic event source interface.

```c
// Generic event source
typedef struct event_source_s event_source_t;

typedef enum {
    EVENT_SOURCE_IO,        // File descriptor readiness
    EVENT_SOURCE_WORKER,    // Worker completion
    EVENT_SOURCE_TIMER,     // Timer expiration (future)
    EVENT_SOURCE_SIGNAL     // Unix signal (future)
} event_source_type_t;

// Unified registration
int event_source_register(event_source_t* source, 
                          event_source_type_t type,
                          void* handle,  // fd, notifier, timer, etc.
                          void* context);

// Unified wait
int event_source_wait(event_source_t* source, 
                      event_t* events, 
                      int max_events, 
                      int timeout_ms);
```

### Pros:
1. Single mental model for all event types
2. Easier to add new event sources (timers, signals, etc.)
3. Potential for cross-platform event source portability

### Cons:
1. **Loss of type safety**: Everything becomes `void*`
2. **Conceptual mismatch**:
   - File descriptors need `add/modify/remove` operations
   - Worker notifications are fire-and-forget
   - Timers need `set/cancel` operations
3. **Implementation complexity**: Least-common-denominator API
4. **No performance benefit**: Still dispatches to platform-specific code
5. **Over-abstraction**: Hides important semantic differences

---

## Option 3: Absorb async_notifier into io_reactor

**Proposal**: Add worker notification APIs directly to io_reactor.

```c
// New io_reactor APIs
int io_reactor_create_notifier(io_reactor_t* reactor, uintptr_t completion_key);
int io_reactor_post_notification(io_reactor_t* reactor, 
                                  uintptr_t completion_key, 
                                  uintptr_t data);
```

### Pros:
1. Fewer types (`async_notifier_t` eliminated)
2. Direct association with reactor

### Cons:
1. **Violates separation of concerns**: io_reactor is for I/O, not worker management
2. **Coupling**: Async library would depend on io_reactor internals
3. **Platform inconsistency**:
   - Windows: Natural fit (both use IOCP)
   - POSIX: Forces eventfd into io_reactor (why not regular `io_reactor_add()`?)
4. **Breaks layering**: lib/async would call lib/port directly (circular dependency potential)

---

## Deeper Analysis: Similarities vs Differences

### Similarities (Surface Level)
- Both provide event demultiplexing
- Both have platform-specific implementations
- Both integrate with main event loop
- Both use non-blocking consumption

### Differences (Fundamental)
| Aspect | io_reactor | async_notifier |
|--------|-----------|----------------|
| **Event source** | Kernel (I/O subsystem) | Application (worker threads) |
| **Lifetime** | Persistent (fd registered until removed) | Transient (one-shot notifications) |
| **Cardinality** | One fd → many events over time | One post → one event |
| **Data flow** | Bidirectional (read/write) | Unidirectional (worker → main) |
| **Context** | OS resource (file descriptor) | Application data (completion key) |
| **Blocking source** | Kernel I/O operations | Worker thread blocking calls |

**Conclusion**: These are **fundamentally different abstractions** that happen to share an integration point (the event loop). Unifying them would obscure these differences.

---

## Real-World Analogy

**io_reactor** = Restaurant kitchen order window
- Multiple orders (file descriptors) being prepared simultaneously
- Chef signals when each dish is ready (I/O event)
- Orders can be modified mid-preparation (io_reactor_modify)
- Orders can be cancelled (io_reactor_remove)

**async_notifier** = Restaurant doorbell
- Customer presses doorbell when delivery arrives (worker completion)
- One ring → one delivery (one-shot notification)
- No ongoing relationship after delivery picked up

Both integrate with the **same waiter** (main event loop), but they serve different purposes. Merging the doorbell API into the kitchen window API would be confusing and lose semantic clarity.

---

## Recommendation: Keep Separate with Small Enhancement

### Verdict: **Option 1** (Current Design)

**Keep async_notifier as separate API** for these reasons:
1. ✅ **Already optimally integrated**: Single `io_reactor_wait()` call returns both
2. ✅ **Type safety preserved**: File descriptors vs completion keys are different types
3. ✅ **Clear semantic boundaries**: I/O vs worker completion are distinct concepts
4. ✅ **Platform abstraction works**: Windows IOCP sharing is transparent
5. ✅ **Minimal API surface**: Only 4 async_notifier functions (create, post, poll, destroy)
6. ✅ **Future-proof**: Can add other event sources (timers, signals) without breaking abstraction

### Small Enhancement: Clarify Integration in Documentation

Add explicit guidance on event discrimination:

```c
// Recommended pattern for unified event loop
#define COMPLETION_KEY_BASE_CONSOLE  0x1000
#define COMPLETION_KEY_BASE_DNS      0x2000
#define COMPLETION_KEY_BASE_FILEIO   0x3000

while (running) {
    io_event_t events[64];
    int n = io_reactor_wait(reactor, events, 64, &timeout);
    
    for (int i = 0; i < n; i++) {
        uintptr_t key = events[i].completion_key;
        
        // Discriminate by completion key range
        if (key >= COMPLETION_KEY_BASE_CONSOLE && key < COMPLETION_KEY_BASE_DNS) {
            // Console worker completion
            handle_console_input();
        } else if (key >= COMPLETION_KEY_BASE_DNS && key < COMPLETION_KEY_BASE_FILEIO) {
            // DNS worker completion
            handle_dns_result();
        } else if (events[i].context != NULL && is_socket(events[i].context)) {
            // Socket I/O event
            handle_socket_event(&events[i]);
        }
    }
}
```

---

## Alternative: Future Generic Event Source (Low Priority)

If Neolith later needs to support:
- POSIX signals as events
- Timer expirations as events
- User-defined event sources

Then consider a **higher-level** event dispatcher above io_reactor and async_notifier:

```c
typedef struct event_dispatcher_s event_dispatcher_t;

event_dispatcher_t* dispatcher = event_dispatcher_create();
event_dispatcher_add_source(dispatcher, io_reactor_as_source(reactor));
event_dispatcher_add_source(dispatcher, async_notifier_as_source(notifier));
event_dispatcher_add_source(dispatcher, timer_as_source(timer));

// Unified wait over all sources
event_t events[128];
int n = event_dispatcher_wait(dispatcher, events, 128, timeout);
```

But this is **premature abstraction**. Current two-API model (io_reactor + async_notifier) is sufficient and clear.

---

## Impact on Implementation Plan

**No changes required**. Current design is optimal:

1. **Phase 1**: Implement async_queue, async_worker, port_sync (lib/port)
2. **Phase 2**: Implement async_notifier with io_reactor integration
   - Windows: `async_notifier_create(io_reactor_get_iocp(reactor))`
   - POSIX: `async_notifier_create(reactor)` → internally calls `io_reactor_add(eventfd)`
3. **Phase 3**: Use in console worker, DNS worker, etc.

**Integration code remains clean**:
```c
// Single event loop handles both I/O and worker completions
io_event_t events[64];
int n = io_reactor_wait(reactor, events, 64, &timeout);
for (int i = 0; i < n; i++) {
    dispatch_event(&events[i]);  // Discriminate by context or completion_key
}
```

---

## Conclusion

**Keep async_notifier and io_reactor as separate APIs.**

They represent different semantic domains (I/O readiness vs worker completion) that happen to share an event loop integration point. The current design:
- ✅ Preserves type safety
- ✅ Maintains clear separation of concerns
- ✅ Already unified where it matters (consumption via `io_reactor_wait`)
- ✅ Allows independent evolution of each component
- ✅ Minimal API surface overhead (4 functions)

**No unification needed.** The "common API" is already present: **`io_reactor_wait()` returns events from both sources**.

---

---

## ADDENDUM: Semantic Expansion Analysis

**Question**: Does `async_notifier` provide semantic expansion, or should io_reactor provide posting semantics directly?

### Current Calling Pattern

```c
// Setup
io_reactor_t* reactor = io_reactor_create();
async_notifier_t* notifier = async_notifier_create(io_reactor_get_iocp(reactor));

// Worker posts notification
async_notifier_post(notifier, WORKER_KEY, data);
  ↓ (internally calls PostQueuedCompletionStatus or write(eventfd))

// Main thread waits
int n = io_reactor_wait(reactor, events, max_events, timeout);
                        ↑
                        This is the ACTUAL event source
```

### Key Observation: async_notifier is a Producer, io_reactor is the Consumer

**async_notifier role**:
- Wrapper around posting mechanism
- No event demultiplexing logic
- Simply writes to reactor's event queue

**io_reactor role**:
- The actual event demultiplexer
- Returns events from ALL sources (I/O + worker completions)
- Owns the event loop

### Semantic Analysis: Is async_notifier an Abstraction Layer or Helper?

#### Current Design (Two Types)
```c
async_notifier_t* notifier;  // Helper for posting
io_reactor_t* reactor;        // Event demultiplexer
```

**Pattern**:
- notifier is a "producer helper"
- reactor is the "consumer"
- Two objects for one semantic operation (posting events to reactor)

#### Alternative: Reactor Provides Posting API Directly

```c
// Setup - only reactor needed
io_reactor_t* reactor = io_reactor_create();

// Worker posts completion DIRECTLY to reactor
io_reactor_post_completion(reactor, WORKER_KEY, data);
  // Windows: PostQueuedCompletionStatus(reactor->iocp, ...)
  // POSIX: write(reactor->wakeup_eventfd, ...)

// Main thread waits (unchanged)
int n = io_reactor_wait(reactor, events, max_events, timeout);
```

### Pros of Absorbing into io_reactor

1. **Single object model**: One reactor, not reactor + notifier
2. **Clearer ownership**: Reactor owns both consumption AND production
3. **Naming consistency**:
   ```c
   io_reactor_add()      // Add I/O source
   io_reactor_post()     // Add completion source
   io_reactor_wait()     // Consume all sources
   ```
4. **Platform abstraction in right place**: 
   - Windows: IOCP posting is reactor's job
   - POSIX: eventfd creation belongs with reactor initialization
5. **No intermediate object lifetime management**: Don't create/destroy notifier separately
6. **Better symmetry**:
   ```c
   // Current asymmetry
   io_reactor_add(reactor, fd, ...);           // Register I/O source
   async_notifier_post(notifier, key, data);   // Post completion (different object!)
   io_reactor_wait(reactor, ...);              // Consume from reactor
   
   // Proposed symmetry
   io_reactor_add(reactor, fd, ...);           // Register I/O source
   io_reactor_post(reactor, key, data);        // Post completion
   io_reactor_wait(reactor, ...);              // Consume all
   ```

### Cons of Absorbing into io_reactor

1. **Coupling**: lib/async would directly depend on lib/port/io_reactor
   - Currently: async_notifier is optional (can use async_queue without it)
   - Proposed: io_reactor becomes hard dependency for worker threads

2. **Conceptual purity**: io_reactor is for "I/O", not generic events
   - Counter-argument: IOCP is ALREADY generic (Windows uses it for everything)
   - Counter-argument: eventfd is just a file descriptor (already fits I/O model)

3. **API surface bloat**: io_reactor gains thread-safe posting functions
   - Counter-argument: Only 1 function added (`io_reactor_post`)

4. **Worker thread access**: Workers call io_reactor functions directly
   - Counter-argument: `io_reactor_wakeup()` already thread-safe
   - Counter-argument: io_reactor is already used from multiple contexts

### Implementation Comparison

#### Current: async_notifier (Wrapper Object)

```c
// lib/async/async_notifier_win32.c
struct async_notifier_s {
    HANDLE iocp_handle;  // Borrowed from reactor
};

bool async_notifier_post(async_notifier_t* notifier, uintptr_t key, uintptr_t data) {
    return PostQueuedCompletionStatus(notifier->iocp_handle, (DWORD)data, key, NULL);
}

// Usage
async_notifier_t* notifier = async_notifier_create(io_reactor_get_iocp(reactor));
async_notifier_post(notifier, key, data);
async_notifier_destroy(notifier);
```

#### Proposed: io_reactor Direct API

```c
// lib/port/io_reactor_win32.c (add to existing file)
int io_reactor_post_completion(io_reactor_t* reactor, 
                                uintptr_t completion_key, 
                                uintptr_t data) {
    return PostQueuedCompletionStatus(reactor->iocp_handle, 
                                      (DWORD)data, 
                                      completion_key, 
                                      NULL) ? 0 : -1;
}

// Usage
io_reactor_post_completion(reactor, key, data);
```

**Difference**: 
- Eliminates `async_notifier_t` type entirely
- Reduces from 4 functions to 1 function
- No separate object lifecycle

### POSIX Eventfd Consideration

**Current Design**:
```c
// async_notifier creates and registers eventfd
async_notifier_t* notifier = async_notifier_create(reactor);
  ↓
  1. Create eventfd
  2. Call io_reactor_add(reactor, eventfd, context, EVENT_READ)
  3. Store reactor and eventfd in notifier struct

// Post writes to eventfd
async_notifier_post(notifier, key, data);
  ↓ write(notifier->eventfd, packed_value, 8)
```

**Proposed Design**:
```c
// Reactor creates internal eventfd during init
io_reactor_t* reactor = io_reactor_create();
  ↓ (internally creates wakeup_eventfd on POSIX)

// Post writes to reactor's eventfd
io_reactor_post_completion(reactor, key, data);
  ↓ write(reactor->wakeup_eventfd, packed_value, 8)

// On EVENT_READ from wakeup_eventfd:
//   - Read packed key|data
//   - Return as io_event_t with completion_key set
```

**Key Change**: wakeup_eventfd becomes **dual-purpose**:
1. `io_reactor_wakeup()` - generic wakeup (key=0, data=0)
2. `io_reactor_post_completion()` - worker completion (key≠0, data=arbitrary)

This is **cleaner** - one eventfd serves both purposes.

### Recommendation: **Absorb async_notifier into io_reactor** ✅

#### Proposed New io_reactor API

```c
// lib/port/io_reactor.h

/**
 * @brief Post a completion event to the reactor (thread-safe).
 * 
 * This allows worker threads to post completion events that will be
 * returned by io_reactor_wait(). The event appears with the specified
 * completion_key and data values.
 * 
 * @param reactor The reactor instance.
 * @param completion_key User-defined key to identify completion type.
 * @param data User-defined data (platform-dependent size constraints).
 * @return 0 on success, -1 on failure.
 * 
 * @note This function is thread-safe and non-blocking.
 * @note On POSIX, data is limited to 32 bits (upper 32 bits of uint64_t).
 * 
 * Example:
 * @code
 *   // Worker thread
 *   void* worker_proc(void* ctx) {
 *       io_reactor_t* reactor = (io_reactor_t*)ctx;
 *       
 *       // Do blocking work
 *       result_t result = perform_dns_lookup(...);
 *       
 *       // Post completion
 *       io_reactor_post_completion(reactor, DNS_COMPLETION_KEY, result.status);
 *       return NULL;
 *   }
 *   
 *   // Main thread
 *   io_event_t events[64];
 *   int n = io_reactor_wait(reactor, events, 64, &timeout);
 *   for (int i = 0; i < n; i++) {
 *       if (events[i].completion_key == DNS_COMPLETION_KEY) {
 *           process_dns_result((int)events[i].bytes_transferred);
 *       }
 *   }
 * @endcode
 */
int io_reactor_post_completion(io_reactor_t* reactor, 
                                uintptr_t completion_key,
                                uintptr_t data);
```

#### Migration Path

**Phase 1**: Add `io_reactor_post_completion()` alongside existing `async_notifier`
```c
// Both APIs coexist
async_notifier_post(notifier, key, data);  // Old API
io_reactor_post_completion(reactor, key, data);  // New API (preferred)
```

**Phase 2**: Update documentation to prefer `io_reactor_post_completion()`

**Phase 3**: (Optional) Deprecate `async_notifier` in favor of direct reactor posting

#### Updated Architecture

**Before**:
```
lib/async/               lib/port/
├── async_queue          ├── io_reactor
├── async_worker         ├── port_sync
└── async_notifier ───→  └── (uses io_reactor)
```

**After**:
```
lib/async/               lib/port/
├── async_queue          ├── io_reactor
└── async_worker              ├── io_reactor_wait()
                              ├── io_reactor_add()
                              └── io_reactor_post_completion() ← NEW
```

**Benefit**: Async library depends on port for sync primitives, but NOT for event posting.

### Why This is Better

1. **Semantic clarity**: "Post completion to reactor" is exactly what's happening
2. **Object model**: One reactor handles all event sources (I/O + completions)
3. **Platform abstraction**: Reactor already abstracts IOCP vs eventfd
4. **Simpler API**: Eliminate 4 functions, add 1 function, net -3
5. **Better naming**: `io_reactor_post_completion` vs `async_notifier_post`
6. **POSIX optimization**: Single eventfd for wakeup AND completions
7. **Reduced coupling**: async library doesn't need notifier abstraction

### Counter-Argument: Keep async_notifier for API Isolation

**If** you want async library to be **completely independent** of io_reactor:

```c
// Pure async library (no reactor dependency)
async_queue_t* queue = async_queue_create(...);
async_worker_t* worker = async_worker_create(...);

// Optionally integrate with reactor via notifier
#ifdef USE_REACTOR
    async_notifier_t* notifier = async_notifier_create(reactor);
    async_notifier_post(notifier, key, data);
#endif
```

**But this is artificial**: In practice, Neolith ALWAYS uses io_reactor. The optional abstraction adds complexity without benefit.

---

## FINAL RECOMMENDATION: Absorb async_notifier into io_reactor

### Rationale

1. **async_notifier is not a semantic abstraction** - it's a thin wrapper around reactor posting
2. **Reactor already owns event demultiplexing** - posting is natural extension
3. **Platform abstraction belongs in lib/port** - reactor is already there
4. **Simpler mental model**: One object (reactor) for all event loop operations
5. **Better API naming**: `io_reactor_post_completion` describes what it does
6. **Implementation is trivial**: Just expose what's already internal

### Updated Async Library Components

1. **async_queue** - Thread-safe message queue (uses port_sync)
2. **async_worker** - Managed worker threads
3. ~~**async_notifier**~~ → **REMOVED** (use `io_reactor_post_completion` instead)

### Worker Thread Pattern (Updated)

```c
// Worker thread context includes reactor
typedef struct {
    async_queue_t* queue;
    io_reactor_t* reactor;  // Direct reactor access
} worker_context_t;

void* worker_proc(void* ctx) {
    worker_context_t* wctx = (worker_context_t*)ctx;
    
    while (!async_worker_should_stop(async_worker_current())) {
        char data[512];
        // Blocking read
        size_t len = read_blocking_source(data, sizeof(data));
        
        // Enqueue result
        async_queue_enqueue(wctx->queue, data, len);
        
        // Notify reactor DIRECTLY
        io_reactor_post_completion(wctx->reactor, WORKER_KEY, len);
    }
    
    return NULL;
}
```

**This is cleaner, clearer, and more direct.**

---

---

## ADDENDUM 2: Semantic Layer Re-evaluation

**Question**: If io_reactor_wait() handles workers AND I/O events, should it be refactored as async library API (high-level abstraction) instead of portability implementation?

### The Core Issue: What IS io_reactor Semantically?

**Current classification**: `lib/port/io_reactor` - "Portability layer for I/O multiplexing"

**Reality after adding worker completions**:
- Handles I/O readiness events (sockets, files)
- Handles worker completion events (thread notifications)
- Handles timer events (via platform mechanisms)
- **This is not "I/O reactor" - this is "event loop" or "async runtime"**

### Semantic Mismatch Analysis

#### What "lib/port" Should Contain

**Definition**: Platform-specific implementations of low-level primitives

**Examples** (correct):
- `port_sync` - Mutexes, condition variables (CRITICAL_SECTION vs pthread_mutex)
- `port_timer` - High-resolution timers (Windows timer vs timer_create)
- `port_realpath` - Path canonicalization (Windows vs POSIX)

**Pattern**: Thin wrappers around OS primitives, one-to-one mapping

#### What "lib/async" Should Contain

**Definition**: High-level async abstractions built on platform primitives

**Examples** (correct):
- `async_queue` - Thread-safe message queue (uses port_sync)
- `async_worker` - Managed worker threads (uses pthread/Windows threads)

**Pattern**: Higher-level semantics, composition of primitives

### Where Does io_reactor Belong?

**Current state**:
```
lib/port/io_reactor
├── Wraps IOCP (Windows)
├── Wraps epoll (Linux)
├── Wraps poll (fallback)
└── Demultiplexes I/O + worker completions + timers
```

**This is TWO things**:
1. **Platform abstraction** (IOCP vs epoll vs poll) → belongs in lib/port
2. **Event loop runtime** (all async event sources) → belongs in lib/async

### Three Refactoring Options

#### Option 1: Keep in lib/port, Rename to "event_loop"

```
lib/port/event_loop.{h,c}  // Renamed from io_reactor
├── event_loop_create()    // Was: io_reactor_create()
├── event_loop_add_io()    // Was: io_reactor_add()
├── event_loop_post_completion()  // New
└── event_loop_wait()      // Was: io_reactor_wait()
```

**Pros**:
- Honest naming (it IS an event loop)
- Platform code stays in lib/port
- Minimal file moves

**Cons**:
- lib/port contains high-level semantics (weird layering)
- Other lib/port modules are thin wrappers, this is not
- "Portability layer" containing application runtime?

#### Option 2: Move to lib/async as "async_runtime"

```
lib/async/async_runtime.{h,c}  // Moved from lib/port/io_reactor
├── async_runtime_create()
├── async_runtime_add_io()
├── async_runtime_post_completion()
└── async_runtime_wait()

// Platform implementations
lib/async/async_runtime_iocp.c     // Windows
lib/async/async_runtime_epoll.c    // Linux
lib/async/async_runtime_poll.c     // Fallback
```

**Pros**:
- Correct semantic layer (async runtime IS lib/async)
- Matches industry naming (libuv, tokio, async-std all have "runtime")
- lib/async becomes cohesive "async subsystem"

**Cons**:
- lib/async now contains platform-specific code
- More files to move
- CMake changes needed

#### Option 3: Split into Two Layers

```
lib/port/io_multiplexer.{h,c}      // LOW-LEVEL: Platform I/O primitives
├── io_multiplexer_create()        // Returns IOCP/epoll/poll handle
├── io_multiplexer_add()
└── io_multiplexer_wait()          // Pure I/O, no worker completions

lib/async/async_runtime.{h,c}      // HIGH-LEVEL: Event loop
├── async_runtime_create()         // Uses io_multiplexer internally
├── async_runtime_add_io()         // Delegates to io_multiplexer
├── async_runtime_post_completion() // Manages completion queue
└── async_runtime_wait()           // Combines I/O + completions
```

**Pros**:
- Clean separation of concerns
- lib/port stays thin (pure platform wrapper)
- lib/async owns high-level semantics

**Cons**:
- Most complex refactoring
- Extra indirection layer
- Overkill for Neolith's needs?

### Lifecycle Implications

#### Current io_reactor Lifecycle

```c
// Main backend initialization
io_reactor_t* reactor = io_reactor_create();

// Add I/O sources (sockets)
io_reactor_add(reactor, telnet_fd, socket_obj, EVENT_READ);

// Workers post completions
io_reactor_post_completion(reactor, WORKER_KEY, data);  // NEW API

// Event loop
while (running) {
    io_event_t events[64];
    int n = io_reactor_wait(reactor, events, 64, &timeout);
    
    for (int i = 0; i < n; i++) {
        if (events[i].completion_key == WORKER_KEY) {
            // Worker completion
        } else {
            // I/O event
        }
    }
}

// Shutdown
io_reactor_destroy(reactor);
```

**Question**: Should reactor creation be part of async library initialization?

#### Proposed: Async Runtime Lifecycle

```c
// lib/async/async_runtime.h

typedef struct async_runtime_s async_runtime_t;

// Initialize async subsystem (replaces io_reactor_create)
async_runtime_t* async_runtime_init(void);

// I/O source management
int async_runtime_add_io(async_runtime_t* rt, socket_fd_t fd, 
                         void* context, int events);
int async_runtime_modify_io(async_runtime_t* rt, socket_fd_t fd, int events);
int async_runtime_remove_io(async_runtime_t* rt, socket_fd_t fd);

// Worker completion posting
int async_runtime_post_completion(async_runtime_t* rt, 
                                   uintptr_t key, uintptr_t data);

// Event loop (blocking)
int async_runtime_wait(async_runtime_t* rt, io_event_t* events, 
                       int max_events, struct timeval* timeout);

// Wakeup from other thread (non-blocking)
int async_runtime_wakeup(async_runtime_t* rt);

// Shutdown
void async_runtime_shutdown(async_runtime_t* rt);
```

**Benefit**: Clear ownership - async runtime manages event loop lifecycle

### Integration with Backend

**Current** (lib/port/io_reactor):
```c
// src/backend.c
#include "port/io_reactor.h"

void backend_init(void) {
    g_reactor = io_reactor_create();  // Feels wrong - backend owns "port" layer?
}

void backend_loop(void) {
    io_reactor_wait(g_reactor, ...);  // Backend uses portability primitive directly
}
```

**Proposed** (lib/async/async_runtime):
```c
// src/backend.c
#include "async/async_runtime.h"

void backend_init(void) {
    g_runtime = async_runtime_init();  // Clear: backend owns async runtime
}

void backend_loop(void) {
    async_runtime_wait(g_runtime, ...);  // Backend runs async event loop
}
```

**Semantic improvement**: Backend owns the async runtime, not a "portability primitive"

### Cross-Platform Perspective

**Windows**: IOCP is Microsoft's async runtime (not just I/O!)
- File I/O completions
- Socket I/O completions  
- Thread pool completions
- Timer completions
- **PostQueuedCompletionStatus for ANY async event**

**POSIX**: No native async runtime, we build one
- epoll/poll for I/O
- eventfd for completions
- timerfd for timers

**Current design mirrors Windows** (good!) but lib/port is wrong layer.

**lib/async is the right layer** because:
1. Windows async runtime is first-class OS concept (IOCP)
2. POSIX async runtime is our construction (epoll + eventfd + timers)
3. Both implement same high-level semantics (event loop)

### Component Dependency After Refactoring

**Option 2 (lib/async owns runtime)**:
```
src/backend ──→ lib/async/async_runtime ──→ lib/port (IOCP/epoll wrappers)
                          ↑
                          │
            lib/async/async_worker
            lib/async/async_queue
```

**Clean layering**:
- src/backend depends on lib/async (high-level async APIs)
- lib/async depends on lib/port (platform primitives)
- lib/port has NO dependencies (pure wrappers)

**Current messy layering**:
```
src/backend ──→ lib/port/io_reactor  (HIGH-LEVEL semantics in LOW-LEVEL layer?!)
                          ↑
                          │
            lib/async/async_notifier (wraps reactor posting)
```

### Recommendation: **Option 2 - Move to lib/async as async_runtime** ✅

#### Rationale

1. **Semantic correctness**: Event loop IS the async runtime, belongs in lib/async
2. **Industry standard**: libuv, tokio, async-std all have "runtime" in async library
3. **Lifecycle clarity**: Backend owns async runtime, not "I/O reactor"
4. **Honest naming**: It's not an I/O reactor anymore (handles workers, timers, etc.)
5. **Clean layering**: lib/async (high-level) → lib/port (low-level)
6. **Windows alignment**: IOCP is Microsoft's async runtime, our code should reflect this

#### Migration Path

**Phase 1**: Rename in place (lib/port)
```c
// lib/port/io_reactor.h → lib/port/event_loop.h (temporary)
event_loop_t* event_loop_create(void);
int event_loop_wait(...);
int event_loop_post_completion(...);  // Add new API
```

**Phase 2**: Move to lib/async
```c
// lib/async/async_runtime.h (final location)
async_runtime_t* async_runtime_init(void);
int async_runtime_wait(...);
int async_runtime_post_completion(...);
```

**Phase 3**: Update all references
- src/backend.c
- lib/socket/*.c
- docs/internals/*.md
- tests/**/

#### File Structure After Refactoring

```
lib/async/
├── async_runtime.h              # Public API (was io_reactor.h)
├── async_runtime_iocp.c         # Windows implementation
├── async_runtime_epoll.c        # Linux implementation
├── async_runtime_poll.c         # Fallback implementation
├── async_queue.h/.c             # Message queue
├── async_worker.h               # Worker thread abstraction
├── async_worker_win32.c
└── async_worker_pthread.c

lib/port/
├── port_sync.h/.c               # Mutexes/events (internal to async)
├── port_timer.h/.c              # High-res timers
└── (other platform utilities)
```

#### Updated Architecture Diagram

```
┌─────────────────────────────────────────────────┐
│              src/backend.c                       │
│  (main event loop, LPC execution)                │
└────────────────┬────────────────────────────────┘
                 │
                 ↓
┌─────────────────────────────────────────────────┐
│           lib/async/ (ASYNC RUNTIME)             │
├─────────────────────────────────────────────────┤
│ async_runtime_init()        [Event Loop]         │
│ async_runtime_add_io()      ├─ I/O sources       │
│ async_runtime_post_completion() ├─ Completions   │
│ async_runtime_wait()        └─ Timers            │
│                                                   │
│ async_worker                [Worker Threads]     │
│ async_queue                 [Message Passing]    │
└────────────────┬────────────────────────────────┘
                 │
                 ↓
┌─────────────────────────────────────────────────┐
│        lib/port/ (PLATFORM PRIMITIVES)           │
├─────────────────────────────────────────────────┤
│ port_sync (mutexes/events)                       │
│ port_timer (high-res timers)                     │
│ Windows: IOCP, CRITICAL_SECTION, Events          │
│ POSIX: epoll, pthread_mutex, eventfd             │
└─────────────────────────────────────────────────┘
```

**Key change**: lib/async owns the runtime, lib/port provides primitives

### Benefits of This Refactoring

1. **Semantic honesty**: Names match reality (async runtime, not I/O reactor)
2. **Clear ownership**: Backend owns async runtime (not portability layer)
3. **Industry alignment**: Matches Rust tokio, Node.js libuv terminology
4. **Better encapsulation**: Async runtime internal details hidden in lib/async
5. **Logical dependencies**: High-level (lib/async) → low-level (lib/port)
6. **Future-proof**: Adding WebSocket server, HTTP client, etc. fits naturally in async runtime

### Counter-Argument: Keep in lib/port

**If** you want to keep lib/port as "any platform abstraction":

**Argument**: IOCP/epoll ARE platform differences, so lib/port is correct

**Counter**: But the SEMANTICS are high-level (event loop), not low-level (syscall wrapper)

**Example distinction**:
- `port_mutex_lock()` - Thin wrapper around pthread_mutex_lock/EnterCriticalSection ✅ lib/port
- `async_runtime_wait()` - Complex logic for event demultiplexing, worker coordination ✅ lib/async

The reactor/runtime has too much semantic weight to be a "portability primitive".

---

## FINAL RECOMMENDATION: Refactor io_reactor → async_runtime in lib/async ✅

### Summary

1. **Move** `lib/port/io_reactor.{h,c}` → `lib/async/async_runtime.{h,c}`
2. **Rename** APIs: `io_reactor_*` → `async_runtime_*`
3. **Add** `async_runtime_post_completion()` for worker notifications
4. **Remove** `async_notifier` (redundant with runtime posting API)
5. **Update** all call sites in src/backend.c, lib/socket/*, tests/*

### Justification

The object that handles I/O events, worker completions, and timers is not a "portability primitive" - it's the **async runtime**. It belongs in lib/async as the foundational abstraction for asynchronous operations.

This aligns Neolith with industry-standard terminology and creates cleaner architectural boundaries.

---

## References

- [async-library.md](async-library.md) - Complete async library design (needs update for async_runtime)
- [lib/port/io_reactor.h](../../lib/port/io_reactor.h) - Current location (to be moved)
- [Windows IOCP Documentation](https://learn.microsoft.com/en-us/windows/win32/fileio/i-o-completion-ports)
- [Linux eventfd(2)](https://man7.org/linux/man-pages/man2/eventfd.2.html)
- [libuv Design Overview](https://docs.libuv.org/en/v1.x/design.html) - Similar async runtime architecture
- [Tokio Runtime](https://docs.rs/tokio/latest/tokio/runtime/) - Rust async runtime model
