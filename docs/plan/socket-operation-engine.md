# Socket Operation Engine Plan

**Status**: Planned  
**Priority**: High  
**Dependencies**: Async runtime (`lib/async`), socket efun compatibility (`lib/socket`, `lib/efuns/sockets.c`)

## Objective

Build a core async socket operation engine that aligns socket efun lifecycle with `async_runtime` and async DNS, while preserving existing LPC socket efun semantics.

**First goal**: lock down current socket efun behavior with tests before refactoring internals.

## Scope

In scope:
- Socket efun behavior baseline tests (current semantics)
- Internal operation lifecycle engine for outbound socket workflows
- Async runtime alignment for LPC socket descriptors
- Async DNS integration with flood protection and bounded resource usage

Out of scope (for this plan):
- New high-level mudlib efuns (CURL-like API)
- Large protocol features (HTTP parser, redirects, cookies)

## Design Principles

1. Preserve LPC compatibility first; improve internals second.
2. Keep interpreter state single-threaded; workers never touch LPC runtime data.
3. Use bounded queues and admission control for DNS and operations.
4. Guarantee deterministic terminal completion (success/fail/timeout/cancel exactly once).
5. Keep C interface stable while internals evolve.

## Execution Checklist

Use this section as the active tracker. Keep milestone details below as the design source of truth.

### Stage 1 Checklist: Behavior Lockdown (Tests First)

- [x] Create `tests/test_socket_efuns/` target and CMake wiring.
- [x] Add baseline compatibility tests for create/bind/listen/accept.
- [x] Add baseline compatibility tests for connect/write/close.
- [x] Add baseline compatibility tests for release/acquire.
- [x] Add callback ordering and return-code assertion cases.
- [x] Add blocked write and flush behavior cases.
- [x] Add security and invalid-state transition guard cases.
- [x] Verify deterministic pass on Linux preset.
- [x] Verify deterministic pass on Windows preset.
- [x] Publish compatibility matrix (scenario -> expected `EE*` and callback sequence).

Stage 1 gate:
- [x] Stage complete when all baseline behavior tests pass on supported platforms.

### Stage 1 Test Matrix (Concrete Baseline)

Use these IDs directly in test names (for example, `SOCK_BHV_001_CreateStream_Success`).

| Test ID | Scenario | Setup | Action | Expected result code | Expected callback sequence | Notes |
|---|---|---|---|---|---|---|
| SOCK_BHV_001 | Create stream socket succeeds | valid owner object | call `socket_create(STREAM, read_cb, close_cb)` | non-negative fd | none immediately | fd becomes usable for subsequent cases |
| SOCK_BHV_002 | Create datagram socket drops close callback path | valid owner object | call `socket_create(DATAGRAM, read_cb, close_cb)` | non-negative fd | none immediately | close callback is ignored for DATAGRAM by current code |
| SOCK_BHV_003 | Invalid mode rejected | valid owner object | call `socket_create(invalid_mode, read_cb, close_cb)` | `EEMODENOTSUPP` | none | mode validation baseline |
| SOCK_BHV_004 | Bind unbound socket succeeds | created stream socket | call `socket_bind(fd, ephemeral_port)` | `EESUCCESS` | none | local address populated |
| SOCK_BHV_005 | Bind already bound socket rejected | bound socket | call `socket_bind(fd, another_port)` | `EEISBOUND` | none | state guard |
| SOCK_BHV_006 | Listen on bound stream socket succeeds | bound stream socket | call `socket_listen(fd, read_cb)` | `EESUCCESS` | none immediately | socket enters LISTEN |
| SOCK_BHV_007 | Listen on datagram socket rejected | bound datagram socket | call `socket_listen(fd, read_cb)` | `EEMODENOTSUPP` | none | mode guard |
| SOCK_BHV_008 | Accept from listening socket succeeds | listening server + connected client | call `socket_accept(listen_fd, read_cb, write_cb)` | non-negative accepted fd | none immediately | accepted fd enters DATA_XFER |
| SOCK_BHV_009 | Accept when not listening rejected | created but non-listening socket | call `socket_accept(fd, read_cb, write_cb)` | `EENOTLISTN` | none | state guard |
| SOCK_BHV_010 | Connect with valid numeric address succeeds | created stream socket + reachable server | call `socket_connect(fd, "127.0.0.1 <port>", read_cb, write_cb)` | `EESUCCESS` or `EEINPROGRESS`-equivalent success path in current implementation | write callback after socket writable | treat async completion via write-select path |
| SOCK_BHV_011 | Connect with malformed address rejected | created stream socket | call `socket_connect(fd, "bad_address", read_cb, write_cb)` | `EEBADADDR` | none | preserve current parser behavior baseline |
| SOCK_BHV_012 | Stream write on connected socket succeeds | connected stream socket | call `socket_write(fd, "payload", 0)` | `EESUCCESS` or `EECALLBACK` | if `EECALLBACK`, then write callback exactly once when flushed | validates partial-write behavior |
| SOCK_BHV_013 | Stream write while blocked rejected | connected stream socket forced into blocked state | call `socket_write(fd, "payload", 0)` | `EEALREADY` | none at call site | blocked-write guard |
| SOCK_BHV_014 | Datagram write without address rejected | datagram socket | call `socket_write(fd, "payload", 0)` | `EENOADDR` | none | datagram address requirement |
| SOCK_BHV_015 | Datagram write with invalid address rejected | datagram socket | call `socket_write(fd, "payload", "bad")` | `EEBADADDR` | none | address parsing guard |
| SOCK_BHV_016 | Close by owner succeeds | open socket owned by caller | call `socket_close(fd)` | `EESUCCESS` | close callback once if configured and callback flag path used | verify no duplicate close callback |
| SOCK_BHV_017 | Close by non-owner rejected | open socket owned by another object | call `socket_close(fd)` | `EESECURITY` | none | owner enforcement |
| SOCK_BHV_018 | Release then acquire succeeds | socket owner A, receiver B | call `socket_release(fd, B, cb)` then `socket_acquire(fd, read_cb, write_cb, close_cb)` as B | `EESUCCESS` on successful handoff | release callback in B once; then normal callbacks owned by B | transfer semantics baseline |
| SOCK_BHV_019 | Acquire without release rejected | socket not marked released | call `socket_acquire(fd, ...)` | `EESOCKNOTRLSD` | none | acquire guard |
| SOCK_BHV_020 | Read callback order on inbound data | connected peer writes data | process read readiness then callback | n/a driver path | read callback receives `(fd, payload)` once per message unit | include MUD, STREAM, DATAGRAM variants |

Callback ordering assertions for all matrix cases:
- Close callback must never fire before terminal write callback for flush-on-close paths.
- Write callback must not fire more than once per blocked write completion.
- No callbacks should fire after socket reaches CLOSED.

Platform notes for expected outcomes:
- Winsock and POSIX may differ on transient connect/send errno, but mapped `EE*` result must match baseline expectations.
- Where the current behavior allows immediate success or callback-based completion, tests should assert accepted set membership instead of a single code.

### Stage 1 Compatibility Matrix Execution Status (2026-03-20)

Implementation source: `tests/test_socket_efuns/test_socket_efuns_behavior.cpp`

Platform execution summary:
- Linux preset: verified passing by maintainer.
- Windows preset (`clang-x64`): verified passing deterministically (3/3 runs, exit 0). Repeated init/deinit cycles work correctly in pedantic mode.

Current per-case status:

| Test ID | Current test implementation status | Linux run | Windows run |
|---|---|---|---|
| SOCK_BHV_001 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_002 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_003 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_004 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_005 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_006 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_007 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_008 | Skeleton (`GTEST_SKIP`) | N/A | N/A |
| SOCK_BHV_009 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_010 | Skeleton (`GTEST_SKIP`) | N/A | N/A |
| SOCK_BHV_011 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_012 | Skeleton (`GTEST_SKIP`) | N/A | N/A |
| SOCK_BHV_013 | Skeleton (`GTEST_SKIP`) | N/A | N/A |
| SOCK_BHV_014 | Skeleton (`GTEST_SKIP`) | N/A | N/A |
| SOCK_BHV_015 | Skeleton (`GTEST_SKIP`) | N/A | N/A |
| SOCK_BHV_016 | Skeleton (`GTEST_SKIP`) | N/A | N/A |
| SOCK_BHV_017 | Skeleton (`GTEST_SKIP`) | N/A | N/A |
| SOCK_BHV_018 | Skeleton (`GTEST_SKIP`) | N/A | N/A |
| SOCK_BHV_019 | Skeleton (`GTEST_SKIP`) | N/A | N/A |
| SOCK_BHV_020 | Skeleton (`GTEST_SKIP`) | N/A | N/A |

### Stage 2 Checklist: Core Operation Engine Skeleton

- [ ] Introduce internal operation table with `op_id`, `socket_id`, owner, phase, deadline.
- [ ] Add operation phases and transition validation.
- [ ] Add single terminal-state guard (exactly one terminal completion).
- [ ] Route outbound connect workflow through operation tracking path.
- [ ] Add trace diagnostics for operation lifecycle transitions.
- [ ] Confirm zero regressions against Stage 1 baseline suite.

Stage 2 gate:
- [ ] Stage complete when operation tracking is live and Stage 1 suite remains green.

### Stage 3 Checklist: Async Runtime Alignment

- [ ] Register LPC sockets consistently with `async_runtime`.
- [ ] Modify runtime interests consistently during blocked/unblocked write transitions.
- [ ] Remove LPC sockets cleanly from `async_runtime` during close/final-close.
- [ ] Normalize context-to-socket mapping for event dispatch.
- [ ] Add invariants for duplicate registration and stale dispatch prevention.
- [ ] Run stress test for registration leak detection.
- [ ] Re-run Stage 1 suite to confirm compatibility.

Stage 3 gate:
- [ ] Stage complete when runtime lifecycle is leak-free and baseline semantics are preserved.

### Stage 4 Checklist: Async DNS with Capacity Lockdown

- [ ] Add DNS worker pool and completion posting via `async_runtime_post_completion()`.
- [ ] Add global in-flight DNS cap.
- [ ] Add bounded pending DNS queue.
- [ ] Add per-owner DNS cap.
- [ ] Add optional duplicate lookup coalescing.
- [ ] Add DNS-phase timeout and total operation deadline handling.
- [ ] Add deterministic overload/timeout error mapping.
- [ ] Add DNS telemetry counters (admitted/rejected/timed-out/dedup-hit).
- [ ] Add flood tests for global/per-owner limits.
- [ ] Confirm backend loop remains responsive under DNS timeout and flood scenarios.

Stage 4 gate:
- [ ] Stage complete when DNS is non-blocking, bounded, and stable under adversarial load.

### Stage 5 Checklist: Hardening and Documentation

- [ ] Add race tests for close-during-DNS.
- [ ] Add lifecycle tests for owner destruction during pending operation.
- [ ] Add timeout/cancel race tests.
- [ ] Add operational telemetry in socket status and trace output.
- [ ] Update internals docs for operation lifecycle and DNS policy.
- [ ] Update manual docs for behavior and operator expectations.
- [ ] Add migration notes for future high-level API work.
- [ ] Verify CI stability across target presets.

Stage 5 gate:
- [ ] Stage complete when hardening tests are stable and docs reflect implementation reality.

## Milestones

### Milestone 1: Behavior Lockdown (Tests First)

**Goal**: freeze current socket efun behavior as executable tests.

Tasks:
- Add `tests/test_socket_efuns/` with baseline compatibility cases.
- Add fixtures for loopback client/server interactions and partial-write simulation.
- Capture callback ordering and return-code expectations.
- Ensure tests run on Linux and Windows presets.

Coverage matrix:
- `socket_create`, `socket_bind`, `socket_listen`, `socket_accept`
- `socket_connect`, `socket_write`, `socket_close`
- `socket_release`, `socket_acquire`
- Error mapping (`EE*`) and security checks
- State transition guard cases (invalid transitions)
- Blocked write / flush semantics

Exit criteria:
- Baseline tests are deterministic and pass on supported platforms.
- A compatibility report maps each tested scenario to expected `EE*` results and callback sequence.

### Milestone 2: Core Operation Engine Skeleton

**Goal**: introduce internal operation records without changing LPC-visible behavior.

Tasks:
- Add internal operation object/table with:
  - `op_id`, `socket_id`, owner object, phase, deadline
  - completion payload and error class
- Define phases:
  - `INIT`, `DNS_RESOLVING`, `CONNECTING`, `TRANSFERRING`, `COMPLETED`, `FAILED`, `TIMED_OUT`, `CANCELED`
- Add strict one-terminal-state transition guard.
- Route outbound connect workflow through operation tracking.

Exit criteria:
- No behavior regression versus Milestone 1 tests.
- Operation diagnostics available for tracing/debug.

### Milestone 3: Async Runtime Alignment

**Goal**: make LPC socket event registration lifecycle explicit and safe.

Tasks:
- Register/modify/remove LPC socket descriptors consistently with `async_runtime`.
- Normalize context-to-socket mapping for event dispatch.
- Add invariants:
  - no duplicate registration
  - no stale dispatch after close
  - no missing write interest when blocked

Exit criteria:
- Stress tests show no leaked runtime registrations.
- Milestone 1 tests remain green.

### Milestone 4: Async DNS with Capacity Lockdown

**Goal**: integrate non-blocking DNS with anti-flood protections.

Tasks:
- Implement DNS worker pool and completion posting to `async_runtime`.
- Add admission-control policy:
  - global in-flight cap
  - bounded pending queue
  - per-owner cap
  - optional duplicate lookup coalescing
- Add timeout handling for DNS phase and total operation deadline.
- Map overload and timeout to deterministic socket errors.

Required protections:
- Reject over-capacity requests immediately.
- Never allow unbounded queue growth.
- Collect counters: admitted, rejected-global, rejected-owner, timed-out, dedup-hit.

Exit criteria:
- DNS timeout scenarios do not stall backend loop.
- Flooding attempts remain within configured limits.
- Milestone 1 compatibility tests remain green where behavior is unchanged.

### Milestone 5: Hardening and Documentation

**Goal**: stabilize and document contracts after implementation.

Tasks:
- Add targeted race and lifecycle tests:
  - close during DNS
  - owner destruct during operation
  - cancel/timeout races
- Add operational telemetry to `dump_socket_status` and trace logs where appropriate.
- Update docs:
  - internals architecture
  - manual behavior notes
  - migration notes for future high-level API work

Exit criteria:
- CI stability across target presets.
- Clear, source-linked docs for operation lifecycle and DNS policy.

## Error and Callback Contract Requirements

1. Preserve existing `EE*` semantics for legacy paths unless explicitly revised by design.
2. Keep callback invocation deterministic; no duplicate terminal callbacks.
3. Ensure owner/security checks remain enforced on all operation paths.

## Testing Strategy Notes

1. Prefer focused unit/integration tests in `tests/test_socket_efuns/` before broad end-to-end runs.
2. Add platform-aware cases for winsock vs POSIX error differences.
3. Use controlled test seams for DNS completion to avoid flaky external-network dependencies.

## Risks

1. Hidden legacy coupling between socket table layout and event dispatch context mapping.
2. Platform-specific nonblocking connect/write differences.
3. Callback ordering regressions during state-machine migration.

## Mitigations

1. Keep descriptor-based compatibility layer intact during early milestones.
2. Land refactors behind milestone test gates.
3. Introduce one semantic change class per milestone.

## Timeline (Estimate)

- Milestone 1: 1-2 weeks
- Milestone 2: 1 week
- Milestone 3: 1 week
- Milestone 4: 1-2 weeks
- Milestone 5: 1 week

Total: 5-7 weeks (incremental delivery, test-gated)

## Related Documents

- [Async DNS Integration Plan](async-dns-integration.md)
- [Async Library User Guide](../manual/async.md)
- [Async Library Design](../internals/async-library.md)
- [Socket efun docs](../efuns/socket_connect.md)

