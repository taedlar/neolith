# Async Worker Pattern Reference

Detailed patterns, examples, and API reference for implementing async worker threads with bounded queues in event-driven systems.

## Queue Flags and Behavior

When allocating queues with `async_queue_allocate()`, choose from these strategies:

| Flag | Behavior | Use Case |
|------|----------|----------|
| `ASYNC_QUEUE_DROP_OLDEST` | Discard oldest if full | High-frequency work where recent tasks are more valuable (DNS resolutions, network reads) |
| `ASYNC_QUEUE_BLOCK_WRITER` | Block enqueue if full | Backpressure needed (strict ordering, no data loss required) |
| `ASYNC_QUEUE_SIGNAL_ON_DATA` | Signal consumer on first enqueue | Low-frequency, bursty work where consumer may be idle |

**Recommendation for I/O**: Use `DROP_OLDEST` to prevent unbounded queuing under load. Pair with admission control for DOS prevention.

## Admission Control Patterns

### Pattern 1: Global Cap Only

Simplest - limit total in-flight work across all owners:

```c
#define GLOBAL_CAP 64

if (in_flight_count >= GLOBAL_CAP)
  return EESOCKET;

in_flight_count++;
queue_work(task);
```

**Pros**: Simple, fair under light load  
**Cons**: Single bad owner can exhaust the cap

### Pattern 2: Global + Per-Owner Caps (Recommended)

Prevent single owner from starving others:

```c
#define GLOBAL_CAP 64
#define PER_OWNER_CAP 8

int owner_id = task->owner_id;
int owner_pending = count_pending_for_owner(owner_id);

if (in_flight_count >= GLOBAL_CAP)
  return EESOCKET;  /* Global exhausted */

if (owner_pending >= PER_OWNER_CAP)
  return EESOCKET;  /* Owner has too many pending */

in_flight_count++;
mark_owner_pending(owner_id);
queue_work(task);
```

**Pros**: Fair, prevents DOS, bounded per-owner resources  
**Cons**: Slightly more bookkeeping

### Pattern 3: Weighted Priorities

Different caps for different work types:

```c
#define DNS_GLOBAL_CAP 64
#define DNS_PER_OWNER_CAP 8
#define FILE_IO_GLOBAL_CAP 32
#define FILE_IO_PER_OWNER_CAP 4

switch (work_type) {
  case WORK_DNS:
    global_cap = DNS_GLOBAL_CAP;
    owner_cap = DNS_PER_OWNER_CAP;
    break;
  case WORK_FILE_IO:
    global_cap = FILE_IO_GLOBAL_CAP;
    owner_cap = FILE_IO_PER_OWNER_CAP;
    break;
}
```

**Pros**: Fine-grained resource allocation  
**Cons**: Complexity, tuning overhead

## Completion Callback Patterns

### Pattern 1: Direct State Transition

Result directly drives state machine:

```c
static void process_completions(void) {
  result_t *result;

  while ((result = async_queue_dequeue(results, 0)) != NULL) {
    object_t *obj = find_object_by_id(result->object_id);
    if (!obj || obj->flags & O_DESTRUCTED) {
      in_flight--;
      mp_free(result);
      continue;
    }

    if (result->success) {
      obj->state = RESOLVED;
      obj->data = result->data;
    } else {
      obj->state = FAILED;
      obj->error = result->error;
    }

    in_flight--;
    mp_free(result);
  }
}
```

**Best for**: Simple state machines, deterministic transitions

### Pattern 2: Callback Application

Invoke LPC apply function on completion:

```c
static void process_completions(void) {
  result_t *result;

  while ((result = async_queue_dequeue(results, 0)) != NULL) {
    object_t *obj = find_object_by_id(result->object_id);
    if (!obj || obj->flags & O_DESTRUCTED) {
      in_flight--;
      mp_free(result);
      continue;
    }

    current_object = obj;

    if (result->success) {
      /* Invoke completion callback */
      apply_function(obj, "dns_callback_success");
    } else {
      apply_function(obj, "dns_callback_failure");
    }

    in_flight--;
    mp_free(result);
  }
}
```

**Best for**: Complex scenarios where object decides next action, LPC control needed

### Pattern 3: Queued Completion (Further Async Work)

Result triggers dependent async work:

```c
static void process_completions(void) {
  result_t *result;

  while ((result = async_queue_dequeue(results, 0)) != NULL) {
    object_t *obj = find_object_by_id(result->object_id);
    if (!obj || obj->flags & O_DESTRUCTED) {
      in_flight--;
      mp_free(result);
      continue;
    }

    if (result->success) {
      /* DNS resolved - queue next work (socket connect) */
      obj->resolved_addr = result->data;
      if (!queue_next_work(obj)) {
        apply_error_callback(obj, "next_work_rejected");
      }
    } else {
      apply_error_callback(obj, result->error);
    }

    in_flight--;
    mp_free(result);
  }
}
```

**Best for**: Multi-stage pipelines (DNS → Connect → TLS Handshake)

## Timeout and Deadline Handling

### Pattern 1: Worker-Side Timeout Check

Worker validates deadline before processing:

```c
static void worker_main(async_worker_t *w, void *arg) {
  work_task_t *task;

  while (!async_worker_should_stop(w)) {
    task = async_queue_dequeue(queue, 100);
    if (!task) continue;

    /* Check if task deadline has passed */
    time_t now = current_time();
    if (now > task->deadline) {
      result_t result = {
        .object_id = task->object_id,
        .success = 0,
        .error = "deadline_exceeded"
      };
      async_queue_enqueue(results, &result, sizeof(result));
      async_runtime_post_completion(runtime, KEY, 1);
      mp_free(task);
      continue;
    }

    /* Perform work with time limit */
    perform_work_with_timeout(task, task->deadline - now);
  }
}
```

**Pros**: Work doesn't run if already expired  
**Cons**: Doesn't prevent enqueue before cleanup

### Pattern 2: Main-Loop Timeout Cleanup

Completion handler reaps stale results:

```c
static void process_completions(void) {
  result_t *result;
  time_t now = current_time();

  while ((result = async_queue_dequeue(results, 0)) != NULL) {
    object_t *obj = find_object_by_id(result->object_id);

    if (!obj || obj->flags & O_DESTRUCTED) {
      in_flight--;
      mp_free(result);
      continue;
    }

    /* Reap if result is too old */
    if (now > result->deadline && !result->success) {
      obj->state = TIMED_OUT;
      obj->error_code = ETIMEDOUT;
      in_flight--;
      mp_free(result);
      continue;
    }

    /* Normal completion handling */
    if (result->success) {
      obj->state = RESOLVED;
    } else {
      obj->state = FAILED;
    }

    in_flight--;
    mp_free(result);
  }
}
```

**Pros**: More flexible, can apply other policies  
**Cons**: Stale results still consume queue space briefly

## Error Handling Strategies

### Errors in Task Queueing (Main Thread)

```c
/* Return error code to caller */
if (in_flight >= GLOBAL_CAP)
  return EESOCKET;  /* Or suitable error for subsystem */

if (!queue_work(&task))
  return EESOCKET;  /* Queue allocation failed */

return EESUCCESS;  /* Work queued, will complete asynchronously */
```

### Errors in Worker Processing

```c
/* Encode as result.success = 0 and optional error code */
result_t result = {
  .object_id = task->object_id,
  .success = 0,
  .error = ECONNREFUSED  /* Or errno value */
};
async_queue_enqueue(results, &result, sizeof(result));
async_runtime_post_completion(runtime, KEY, 1);
```

### Errors in Completion Handling

```c
/* Already have result - apply error callback or transition state */
if (!result->success) {
  obj->state = FAILED;
  if (obj->error_callback) {
    apply_error_callback(obj, obj->error_callback);
  }
}
```

## Testing Async Worker Patterns

### Unit Test: Queue Bounds

```c
TEST(AsyncWorker, QueueRespectsBoundary) {
  async_queue_t *q = async_queue_allocate(8, sizeof(task_t), DROP_OLDEST);
  
  /* Fill queue to capacity */
  for (int i = 0; i < 8; i++) {
    task_t task = {.id = i};
    ASSERT_TRUE(async_queue_enqueue(q, &task, sizeof(task)));
  }
  
  /* Next enqueue should drop oldest or fail based on strategy */
  task_t extra = {.id = 999};
  if (strategy == DROP_OLDEST) {
    ASSERT_TRUE(async_queue_enqueue(q, &extra, sizeof(extra)));
    /* First task should now be gone */
  }
  
  async_queue_free(q);
}
```

### Unit Test: Admission Control

```c
TEST(AsyncWorker, PerOwnerAdmissionControl) {
  init_async_work();
  
  /* Owner 1 queues 8 tasks (at per-owner cap) */
  for (int i = 0; i < 8; i++) {
    ASSERT_TRUE(queue_dns_resolution(/*owner=*/1, "host1.com", 80));
  }
  
  /* 9th attempt should be rejected */
  ASSERT_FALSE(queue_dns_resolution(/*owner=*/1, "host9.com", 80));
  
  /* Owner 2 should still be able to queue */
  ASSERT_TRUE(queue_dns_resolution(/*owner=*/2, "host2.com", 80));
  
  deinit_async_work();
}
```

### Integration Test: Round-Trip

```c
TEST(AsyncWorker, DnsResolutionRoundTrip) {
  init_async_work();
  
  /* Queue DNS work */
  ASSERT_TRUE(queue_dns_resolution(/*socket_id=*/42, "localhost", 8080));
  
  /* Simulate worker processing (in real system, worker thread runs) */
  // ... worker dequeues task, resolves localhost to 127.0.0.1 ...
  // ... worker enqueues result and posts completion ...
  
  /* Main loop processes completion */
  process_dns_completions();
  
  /* Verify socket state changed correctly */
  EXPECT_EQ(lpc_socks[42].state, DATA_XFER);
  EXPECT_EQ(lpc_socks[42].r_addr.sin_addr.s_addr, htonl(INADDR_LOOPBACK));
  
  deinit_async_work();
}
```

## Performance Tuning

### Queue Sizing Heuristic

```
task_queue_size = (expected_qps * max_latency_seconds) * 1.5
results_queue_size = task_queue_size  /* Often similar */
```

Example: 100 DNS requests/sec, 2-second timeout:
```
task_queue = 100 * 2 * 1.5 = 300
```

Round to power of 2 or multiple of 10 for simplicity.

### Worker Count Strategy

- **Single worker**: Sufficient for sequential-only work (file I/O, single extern call)
- **Multiple workers**: Needed for parallel-safe work (DNS resolution, crypto operations)
  - Use thread pool `async_worker_create_pool(worker_count, ...)`
  - Distribute tasks across workers for higher throughput

### Admission Control Tuning

```
GLOBAL_CAP = (expected_concurrent_owners) * (PER_OWNER_CAP)
PER_OWNER_CAP = min(max_work_per_owner, queue_size / expected_owners)
```

Start conservative (low caps), observe queue drop rates, increase if needed.

## References

- **Socket DNS example** (real-world): [lib/socket/socket_efuns.c](../../../lib/socket/socket_efuns.c) `dns_worker_main()`, `queue_dns_resolution()`, `process_dns_completions()`
- **Main loop integration** (real-world): [src/comm.c](../../../src/comm.c) `process_io()` completion key handler
- **Async runtime library**: [lib/async/async_runtime.h](../../../lib/async/async_runtime.h)
- **Design whitepaper**: [docs/internals/async-library.md](../../../docs/internals/async-library.md)
