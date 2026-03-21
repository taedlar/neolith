---
name: async-worker-pattern
description: 'Implement async worker thread patterns with bounded task queues, admission control, and completion posting. Use when adding blocking I/O operations (DNS, file I/O, network calls) that must not block the main event loop. Covers worker thread design, queue management, completion callbacks, and main-loop integration for event-driven architectures.'
---

# Async Worker Pattern Skill

Implement efficient async worker thread patterns to handle blocking I/O operations without stalling the main event loop. This skill covers the complete lifecycle: task queueing, worker execution, bounded queue management, and completion-driven state transitions.

## When to Use This Skill

**Apply this skill when:**
- Adding blocking I/O (DNS, file operations, network calls) to an event-driven driver
- Main loop must remain responsive under load (never block on system calls)
- Worker threads need to post results back without touching interpreter/global state
- You need admission control to prevent resource exhaustion (DOS protection)
- Completion callbacks must transition objects through state machines

**Example scenarios:**
- Socket DNS resolution (hostname → IPv4)
- File I/O (read file contents, check directory)
- External HTTP calls with timeout
- Certificate validation or crypto operations
- Database queries in async drivers

## Prerequisites

- Understanding of event loops and non-blocking I/O
- Async runtime library with these primitives:
  - `async_queue_allocate(size, element_size, flags)` - create bounded queue
  - `async_queue_enqueue(queue, data, size)` - main thread → worker
  - `async_queue_dequeue(queue, timeout_ms)` - worker → consumer
  - `async_worker_create(work_fn, arg)` - spawn worker thread
  - `async_worker_should_stop(worker)` - graceful shutdown check
  - `async_worker_destroy(worker)` - join and cleanup
  - `async_runtime_post_completion(runtime, completion_key, data)` - worker → main loop
- Single-threaded main loop that collects events and dispatches completions

## Core Architecture Pattern

```
┌─────────────────────────────────────────────────────────────┐
│ Main Thread (Non-blocking Event Loop)                       │
│                                                              │
│  1. Detect blocking work needed (e.g., hostname)            │
│  2. Queue task to bounded queue + increment counter         │
│  3. Return to application immediately (EESUCCESS)           │
│  4. Poll for completions in event dispatch:                 │
│     - Check completion_key in event                         │
│     - Drain results queue                                   │
│     - Transition object state (e.g., RESOLVING → CONNECTING)│
└─────────────────────────────────────────────────────────────┘
                         ↑                    ↓
                         │ task queue         │ results queue
                         │ (bounded)          │ (bounded)
                         ↓                    ↑
┌─────────────────────────────────────────────────────────────┐
│ Worker Thread (Blocking Operations OK)                      │
│                                                              │
│  1. Dequeue task with timeout                               │
│  2. Perform blocking work (getaddrinfo, read, etc.)         │
│  3. Format result (status, output data)                     │
│  4. Enqueue result                                          │
│  5. Post completion to main loop (wakes event loop)         │
│  6. Loop until async_worker_should_stop() = true            │
└─────────────────────────────────────────────────────────────┘
```

## Step-by-Step Workflow

### 1. Design Task and Result Structures

Define compact message types for data passing (avoid pointers):

```c
/* Work queued by main thread */
typedef struct {
  int object_id;
  char hostname[64];
  uint16_t port;
  time_t deadline;
} dns_task_t;

/* Result returned by worker */
typedef struct {
  int object_id;
  struct in_addr resolved_addr;  /* or error code as union */
  uint16_t port;
  int success;  /* 1 = success, 0 = failed */
} dns_result_t;
```

**Key principles:**
- Structures are **plain data only** (no pointers, no LPC objects)
- Include enough context for main loop to find the object back (object_id, socket_id, etc.)
- Keep total size small (<256 bytes) for queue efficiency
- Use fixed-size buffers, never variable-length allocations in queue

### 2. Declare Bounded Queues and Worker

```c
/* Global state (file-static to module) */
#define WORK_QUEUE_SIZE 256
#define RESULT_QUEUE_SIZE 256
#define WORK_COMPLETION_KEY 0x574F524B  /* "WORK" */

static async_queue_t *work_queue = NULL;
static async_queue_t *result_queue = NULL;
static async_worker_t *worker = NULL;

/* Admission control counters */
static int in_flight_count = 0;
#define GLOBAL_CAP 64
#define PER_OWNER_CAP 8
```

### 3. Implement Worker Thread Function

Worker function executes in worker thread context (blocking is OK):

```c
static void worker_main(async_worker_t *worker, void *arg) {
  async_queue_t *queue = (async_queue_t *)arg;
  dns_task_t *task;
  dns_result_t result;

  while (!async_worker_should_stop(worker)) {
    /* Dequeue with timeout to allow periodic shutdown checks */
    task = (dns_task_t *)async_queue_dequeue(queue, 100);
    if (task == NULL)
      continue;

    /* BLOCKING OPERATION HERE (OK in worker thread) */
    struct addrinfo hints, *results;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;

    result.object_id = task->object_id;
    result.port = task->port;
    result.success = 0;

    if (getaddrinfo(task->hostname, NULL, &hints, &results) == 0) {
      for (struct addrinfo *entry = results; entry; entry = entry->ai_next) {
        if (entry->ai_family == AF_INET && entry->ai_addr) {
          result.resolved_addr = 
            ((struct sockaddr_in *)entry->ai_addr)->sin_addr;
          result.success = 1;
          break;
        }
      }
      freeaddrinfo(results);
    }

    /* Queue result and post completion to wake main loop */
    async_queue_enqueue(result_queue, &result, sizeof(result));
    async_runtime_post_completion(get_async_runtime(), 
                                   WORK_COMPLETION_KEY, 1);

    mp_free(task);
  }
}
```

**Critical patterns:**
- **Dequeue with timeout** (not 0) to periodically check `async_worker_should_stop()`
- **Never touch shared mutable state** (globals, LPC objects, etc.)
- **Always enqueue result even on error** (use `success` flag for status)
- **Post completion after enqueueing result** (ensures result is available when main loop wakes)
- **Free task memory** if allocator tracks ownership

### 4. Implement Task Queueing (Main Thread Only)

Detect blocking work need and queue task non-blockingly:

```c
static int queue_work(int object_id, const char *hostname, uint16_t port) {
  /* Admission control: check global cap */
  if (in_flight_count >= GLOBAL_CAP)
    return 0;  /* Rejected - too many in-flight */

  /* Admission control: check per-owner cap */
  if (count_pending_for_owner(object_id) >= PER_OWNER_CAP)
    return 0;  /* Rejected - owner has too many pending */

  /* Package and queue the task */
  dns_task_t task;
  task.object_id = object_id;
  strncpy(task.hostname, hostname, sizeof(task.hostname) - 1);
  task.hostname[sizeof(task.hostname) - 1] = '\0';
  task.port = port;
  task.deadline = current_time() + DNS_TIMEOUT;

  if (!async_queue_enqueue(work_queue, &task, sizeof(task)))
    return 0;  /* Queue full - rejected */

  in_flight_count++;
  return 1;  /* Successfully queued */
}
```

### 5. Implement Completion Handler (Main Loop Integration)

Called from main loop when completion_key fires:

```c
static void process_completions(void) {
  dns_result_t *result;

  /* Drain all pending results from queue (0ms timeout = non-blocking) */
  while ((result = (dns_result_t *)async_queue_dequeue(result_queue, 0)) 
         != NULL) {
    int object_id = result->object_id;

    /* Safely access object (it may have been destroyed) */
    object_t *obj = find_object_by_id(object_id);
    if (obj == NULL || obj->flags & O_DESTRUCTED) {
      in_flight_count--;
      mp_free(result);
      continue;
    }

    if (!result->success) {
      /* DNS resolution failed - apply error callback or transition to error state */
      apply_dns_failed_callback(obj);
      in_flight_count--;
      mp_free(result);
      continue;
    }

    /* DNS succeeded - apply resolved address and transition state */
    obj->resolved_addr = result->resolved_addr;
    obj->port = result->port;

    /* Attempt next phase (e.g., connect) */
    if (!attempt_connect(obj, &result->resolved_addr, result->port)) {
      apply_dns_failed_callback(obj);
    }

    in_flight_count--;
    mp_free(result);
  }
}
```

### 6. Integrate with Main Event Loop

Register completion handler in event dispatcher:

```c
/* In process_io() or equivalent main-loop event handler */
for (each async_event in event_list) {
  if (evt->completion_key == CONSOLE_KEY) {
    /* Handle console I/O */
  }
  else if (evt->completion_key == WORK_COMPLETION_KEY) {
    /* Handle async work completions */
    process_completions();
  }
  else if (is_socket_readable(evt)) {
    /* Handle socket I/O */
  }
}
```

### 7. Initialize and Cleanup

Call init/deinit symmetrically:

```c
/* Initialization (once at driver startup) */
static int init_async_work(void) {
  work_queue = async_queue_allocate(WORK_QUEUE_SIZE, sizeof(dns_task_t),
                                     ASYNC_QUEUE_DROP_OLDEST);
  if (!work_queue) return 0;

  result_queue = async_queue_allocate(RESULT_QUEUE_SIZE, sizeof(dns_result_t),
                                       ASYNC_QUEUE_DROP_OLDEST);
  if (!result_queue) {
    async_queue_free(work_queue);
    return 0;
  }

  worker = async_worker_create(worker_main, work_queue);
  if (!worker) {
    async_queue_free(work_queue);
    async_queue_free(result_queue);
    return 0;
  }

  return 1;
}

/* Cleanup (at driver shutdown) */
static void deinit_async_work(void) {
  if (worker) {
    async_worker_destroy(worker);  /* Waits for graceful shutdown */
    worker = NULL;
  }
  if (work_queue) {
    async_queue_free(work_queue);
    work_queue = NULL;
  }
  if (result_queue) {
    async_queue_free(result_queue);
    result_queue = NULL;
  }
  in_flight_count = 0;
}
```

## Common Patterns & Pitfalls

### ✅ DO: Keep Worker Pure (No Shared State)

```c
/* Good: Worker only reads from dequeued task */
static void worker_main(async_worker_t *w, void *arg) {
  dns_task_t *task = async_queue_dequeue(queue, 100);
  const char *hostname = task->hostname;  /* Use local copy */
  struct addrinfo *results = NULL;
  getaddrinfo(hostname, NULL, &hints, &results);  /* ✓ Blocking OK */
  /* ... process results ... */
}
```

### ❌ DON'T: Access Shared Mutable Globals

```c
/* Bad: Worker modifies global shared state */
static void worker_main(async_worker_t *w, void *arg) {
  dns_task_t *task = async_queue_dequeue(queue, 100);
  shared_object_table[task->object_id].status = RESOLVED;  /* ✗ Race! */
}
```

### ✅ DO: Always Post Completion After Enqueueing Result

```c
/* Good ordering */
async_queue_enqueue(result_queue, &result, sizeof(result));  /* First enqueue result */
async_runtime_post_completion(runtime, KEY, 1);              /* Then post completion */
```

### ❌ DON'T: Post Completion Before Enqueueing

```c
/* Bad: Main loop wakes before result is available */
async_runtime_post_completion(runtime, KEY, 1);
async_queue_enqueue(result_queue, &result, sizeof(result));  /* ✗ Race! */
```

### ✅ DO: Check Object Still Valid in Completion Handler

```c
/* Good: Validate before using */
object_t *obj = find_object_by_id(result->object_id);
if (obj == NULL || obj->flags & O_DESTRUCTED) {
  /* Object was destroyed - safely skip */
  in_flight_count--;
  continue;
}
```

### ❌ DON'T: Assume Object Survives Until Completion

```c
/* Bad: Object may have been destroyed */
object_t *obj = &global_objects[result->object_id];  /* ✗ May be invalid */
obj->status = DNS_RESOLVED;
```

### ✅ DO: Use Bounded Admission Control

```c
/* Good: Prevent DOS and resource exhaustion */
if (in_flight_count >= GLOBAL_CAP) return EESOCKET;
if (per_owner_count >= PER_OWNER_CAP) return EESOCKET;
```

### ❌ DON'T: Unbounded Task Queueing

```c
/* Bad: No intake control - queue grows without limit */
async_queue_enqueue(work_queue, task, size);  /* ✗ Can exhaust memory */
```

## Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| Main loop appears to hang | Worker thread blocked main event loop | Verify worker never calls event-loop-unsafe functions; check for async_queue_dequeue(queue, 0) in worker |
| Tasks never process | Worker not running or dequeue failing | Check async_worker_create() returned non-NULL; verify worker dequeue timeout is > 0 |
| Completions not firing | Main loop not checking completion_key | Register completion handler in event dispatcher; verify completion_key is unique |
| Objects already destroyed when completion fires | Timing window | Always validate object with find_object_by_id() + check O_DESTRUCTED flag |
| Queue fills up and tasks DROP | Completion handler too slow or consumer blocked | Increase queue size; check completion handler for inefficiencies; verify no recursive calls |
| Resource leak on shutdown | Worker thread not joining | Call async_worker_destroy() in deinit; verify all mp_alloc() in worker match mp_free() |

## Real-World Example: Socket DNS Resolution

See [lib/socket/socket_efuns.c](../../../lib/socket/socket_efuns.c) Stage 4A implementation for a complete example:

- **Task structure**: `dns_task_t` (socket_id, hostname, port)
- **Result structure**: `dns_result_t` (socket_id, resolved_addr, port, success)
- **Worker function**: `dns_worker_main()` uses `getaddrinfo()` in worker thread
- **Admission control**: Global cap (64), per-owner cap (8)
- **Completion handler**: `process_dns_completions()` drains results and transitions sockets
- **Main loop integration**: DNS_COMPLETION_KEY check in [src/comm.c](../../../src/comm.c#L1141)

Test validation: All 26 socket tests pass; SOCK_DNS_001/002 enabled, SOCK_DNS_003 validates async path.

## References

- [async_runtime library design](../../../docs/internals/async-library.md)
- [Socket operation engine Stage 4A plan](../../../docs/plan/socket-operation-engine.md#stage-4a-hostname-resolution)
- Queue and worker API: [lib/async/async_runtime.h](../../../lib/async/async_runtime.h)

