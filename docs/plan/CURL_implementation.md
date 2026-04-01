# PACKAGE_CURL Implementation Plan

## Description
Implement PACKAGE_CURL, an opt-in feature that adds three LPC efuns for non-blocking REST API calls via libcurl. The implementation leverages Neolith's existing async runtime, worker thread patterns, and per-object callback model (similar to input_to and socket callbacks). Enforce one reusable easy handle per object with max one concurrent transfer per object; auto-cancel on object destruction.

**API Overview**:
- `perform_using(opt, val)` — Configure per-object CURL easy handle (lazy-init, idle-only reconfig)
- `perform_to(fun, flag, ...)` — Non-blocking transfer submit with callback on completion
- `in_perform()` — Status query returning true if current object has active transfer

**Key Constraints**:
- One active transfer per object (additional perform_to calls fail if transfer active)
- Callback receives (success_flag, body_or_error, ...carryover_args) per input_to convention; on success body_or_error is a buffer, on failure it is a string error message
- Transfers cancelled and callback not invoked on object destruction
- Worker thread pool handles curl_multi_perform/curl_multi_poll; main thread does callbacks

---

## Implementation Status

### Phase 1: Build/Package Wiring ✅ COMPLETE
- [x] Add PACKAGE_CURL option to cmake/options.cmake (default OFF)
- [x] Add PACKAGE_CURL #cmakedefine to lib/lpc/options.h.in
- [x] Add libcurl detection to CMakeLists.txt (find_package(CURL REQUIRED) when PACKAGE_CURL=ON)
- [x] Create lib/curl/ library with CMakeLists.txt, curl_efuns.h, curl_efuns.cpp stubs
- [x] Wire curl library linkage into src/CMakeLists.txt and lib/efuns/CMakeLists.txt
- [x] Add curl_efuns.h include to lib/lpc/object.c (guard with #ifdef PACKAGE_CURL)
- [x] Build successful; all three efuns registered with opcodes (F_PERFORM_USING=329, F_PERFORM_TO=330, F_IN_PERFORM=331)

### Phase 2: LPC API & Efun Registration ✅ COMPLETE
- [x] Add efun signatures to lib/lpc/func_spec.c.in behind PACKAGE_CURL guards
  - `void perform_using(mixed, mixed);`
  - `void perform_to(string | function, int,...);`
  - `int in_perform(void);`
- [x] Fixed varargs signature syntax (no parameter names, use `(...);` at end)
- [x] Efun opcode generation validated (edit_source parsed correctly)

### Phase 3: CURL Runtime Subsystem ✅ COMPLETE
**Subtasks**:
1. Define CURL subsystem data structures
   - Global per-object handle pool array (curl_http_t)
   - Owner object tracking and lazy handle initialization
   - Active transfer state / pending callback storage
   - Response/error buffers with size management

2. Implement perform_using and in_perform
   - Lazy handle creation on first perform_using call
   - Option/value pair application to easy handle
   - Reject option mutations during active transfer
   - Inline in_perform status query (zero overhead)

3. Implement perform_to callback submit
   - Validate no active transfer for current object
   - Snapshot callback (string or function pointer) + carryover args
   - Enqueue transfer request to worker task queue
   - Return immediately (non-blocking)

4. Implement async worker thread
   - Worker main loop using async queue dequeue
   - Drive curl_multi_perform/curl_multi_poll for all in-flight handles
   - Enqueue completion results
   - Post completion key to main-thread async runtime

5. Implement main-thread completion drain/dispatch
   - Consume completion records from result queue upon completion key event
   - Skip callback if owner object destructed (check O_DESTRUCTED flag)
   - Invoke callback using safe apply/function-pointer patterns from socket/input precedents
   - Pass (success_flag, body_or_error, ...carryover_args) per contract; body is a buffer on success, string on failure

### Phase 4: Lifecycle & Cancellation Guarantees ✅ COMPLETE
**Subtasks**:
1. Add object destruction cleanup
   - Extend destruct path in lib/lpc/object.c to call close_curl_handles(ob)
   - Cancel any in-flight transfer (graceful shutdown or forced cleanup)
   - Free callback references and handle resources
   - Guard against stale completion postcounts via generation/state token

2. Define completion token safety
   - Add generation ID or state version to per-object handle
   - Include token in completion record
   - Skip completion execution if token does not match (handle was cancelled/reallocated)

3. Add subsystem lifecycle
   - init_curl_subsystem() called during driver startup (register with machine state)
   - deinit_curl_subsystem() called on driver shutdown
   - Graceful worker thread stop and resource cleanup

### Phase 5: Tests & Documentation ✅ COMPLETE
**Subtasks**:
1. Unit and integration tests
   - Argument validation tests (callback type, flags)
   - One-active-transfer enforcement
   - Option persistence across idle periods
   - Callback payload ordering and carryover argument delivery
   - Cancel-on-destruct behavior verification
   - Multi-object concurrency (each object can have one active transfer independently)

2. Documentation
   - [docs/efuns/perform_using.md](docs/efuns/perform_using.md) — option syntax, error contract
   - [docs/efuns/perform_to.md](docs/efuns/perform_to.md) — callback semantics, carryover args, non-blocking guarantee
   - [docs/efuns/in_perform.md](docs/efuns/in_perform.md) — status query contract
   - Update [docs/ChangeLog.md](docs/ChangeLog.md) if release-facing

---

## Current State Handoff

**What's implemented**:
- Complete build wiring: PACKAGE_CURL option, libcurl detection, library setup, efun opcode registration
- Full CURL runtime in lib/curl/curl_efuns.cpp: per-object handle pool, lazy easy-handle init, async task/result queues, curl_multi worker loop, and main-thread callback dispatch
- Event-loop integration in src/comm.c: subsystem init/deinit plus CURL completion drain dispatch
- Destruction-safe cancellation via generation tokens and owner detachment; stale completions are ignored and cleaned up
- Verified clean build of curl and neolith targets

**What's next**:
- Optional follow-up work is now limited to broader integration examples or future option-surface expansion.

**Entry point for follow-up work**:
- Extend [tests/test_efuns/test_curl.cpp](tests/test_efuns/test_curl.cpp) if the CURL API grows beyond the current option set.
- Expand the guarded `curlget` example in [examples/m3_mudlib/user.c](examples/m3_mudlib/user.c) if PACKAGE_CURL becomes part of the default enabled feature set.

**Key reference implementations to mirror**:
- [lib/socket/socket_efuns.c](lib/socket/socket_efuns.c) — per-object handle pool, owner cleanup
- [src/addr_resolver.cpp](src/addr_resolver.cpp) — async worker, queue, completion posting
- [src/command.c](src/command.c) and [src/comm.c](src/comm.c) — input_to callback mechanics
- [docs/internals/async-library.md](docs/internals/async-library.md) — async runtime API reference

---

## Known Dependencies & Integration Points

| Component | File | Interaction |
|-----------|------|-------------|
| **Build system** | [cmake/options.cmake](cmake/options.cmake), [CMakeLists.txt](CMakeLists.txt), [lib/lpc/func_spec.c.in](lib/lpc/func_spec.c.in) | Conditional compilation, opcode generation |
| **Async runtime** | [lib/async/async_runtime.h](lib/async/async_runtime.h), [src/comm.c](src/comm.c) | Task/result queue, completion posting, main-loop polling |
| **Object lifecycle** | [lib/lpc/object.c](lib/lpc/object.c), [src/simulate.c](src/simulate.c) | Destruction cleanup hook integration |
| **Callback dispatch** | [src/apply.c](src/apply.c), [lib/socket/socket_efuns.c](lib/socket/socket_efuns.c) | Safe callback invocation patterns |
| **Efun vector** | [lib/lpc/edit_source.c](lib/lpc/edit_source.c) (opcode generation) | Generated opcode table (efuns_opcod.h) |

---

## Lessons Learned

- Generation tokens need to be stable per submitted transfer, not just per handle slot, otherwise cancel-on-destroy can race late worker completions.
- Keeping callback dispatch on the main thread avoids re-entrancy and stack-discipline problems already solved by safe_apply()/safe_call_function_pointer() patterns used elsewhere in the driver.
- Detached in-flight handles should not free libcurl-owned request data until the worker observes completion; owner detachment plus stale-completion cleanup is the safer boundary.
- For driver efun tests that depend on async completions, a fixture-local async runtime plus loopback server gives deterministic coverage without needing the full network listener startup path.
- `make_lfun_funp_by_name()` plus `push_refed_funp()` is the correct ownership pattern for efun tests that need real LPC function-pointer callbacks.

