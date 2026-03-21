# Socket Operation Engine Plan

**Status**: In progress  
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
- Linux preset (`clang-x64`): verified passing deterministically (exit 0).
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
| SOCK_BHV_008 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_009 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_010 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_011 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_012 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_013 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_014 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_015 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_016 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_017 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_018 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_019 | Implemented (assertions) | Pass | Pass |
| SOCK_BHV_020 | Implemented (assertions) | Pass | Pass |

### Stage 2 Checklist: Core Operation Engine Skeleton

- [x] Introduce internal operation table with `op_id`, `socket_id`, owner, phase, deadline.
- [x] Add operation phases and transition validation.
- [x] Add single terminal-state guard (exactly one terminal completion).
- [x] Route outbound connect workflow through operation tracking path.
- [x] Add trace diagnostics for operation lifecycle transitions.
- [x] Confirm zero regressions against implemented Stage 1 baseline tests (`SOCK_BHV_001`-`SOCK_BHV_020`).
- [x] Add focused Stage 2 lifecycle tests for operation start/phase/terminal clearing (`SOCK_OP_001`-`SOCK_OP_003`).

### Stage 2 Verification Status (2026-03-21)

Implementation source:
- `lib/socket/socket_efuns.c`
- `lib/socket/socket_efuns.h`
- `tests/test_socket_efuns/fixtures.hpp`
- `tests/test_socket_efuns/test_socket_efuns_behavior.cpp`

Current Stage 2 targeted test status:

| Test ID | Scenario | Linux run | Windows run |
|---|---|---|---|
| SOCK_OP_001 | Connect creates tracked operation in `OP_TRANSFERRING` | Pass | Pass |
| SOCK_OP_002 | Malformed address does not create operation record | Pass | Pass |
| SOCK_OP_003 | Close clears operation and duplicate terminal path is inert | Pass | Pass |

### Full Socket Test Target Snapshot (2026-03-21, Linux and Windows `clang-x64`)

Execution scope:
- Full `tests/test_socket_efuns` target subset: `SocketEfunsBehaviorTest.SOCK_BHV_001` through `SocketEfunsBehaviorTest.SOCK_BHV_020`, plus `SocketEfunsBehaviorTest.SOCK_OP_001` through `SocketEfunsBehaviorTest.SOCK_OP_003`.

Result summary:
- Linux: 23 executed, 23 passed, 0 skipped, 0 failed
- Windows: 23 executed, 23 passed, 0 skipped, 0 failed

Stage 1 (`SOCK_BHV_*`) snapshot:
- Passed: `SOCK_BHV_001`, `SOCK_BHV_002`, `SOCK_BHV_003`, `SOCK_BHV_004`, `SOCK_BHV_005`, `SOCK_BHV_006`, `SOCK_BHV_007`, `SOCK_BHV_008`, `SOCK_BHV_009`, `SOCK_BHV_010`, `SOCK_BHV_011`, `SOCK_BHV_012`, `SOCK_BHV_013`, `SOCK_BHV_014`, `SOCK_BHV_015`, `SOCK_BHV_016`, `SOCK_BHV_017`, `SOCK_BHV_018`, `SOCK_BHV_019`, `SOCK_BHV_020`

Stage 2 (`SOCK_OP_*`) snapshot:
- Passed: `SOCK_OP_001`, `SOCK_OP_002`, `SOCK_OP_003`

Stage 2 gate:
- [x] Stage complete when operation tracking is live and Stage 1 suite remains green.

### Stage 3 Checklist: Async Runtime Alignment

- [x] Register LPC sockets consistently with `async_runtime`.
- [x] Modify runtime interests consistently during blocked/unblocked write transitions.
- [x] Remove LPC sockets cleanly from `async_runtime` during close/final-close.
- [x] Normalize context-to-socket mapping for event dispatch.
- [x] Add invariants for duplicate registration and stale dispatch prevention.
- [x] Run stress test for registration leak detection.
- [x] Re-run Stage 1 suite to confirm compatibility.

### Stage 3 Verification Status (2026-03-21)

Implementation source:
- `lib/socket/socket_efuns.c`
- `lib/socket/socket_efuns.h`
- `src/comm.c`
- `tests/test_socket_efuns/fixtures.hpp`
- `tests/test_socket_efuns/test_socket_efuns_behavior.cpp`

Current Stage 3 targeted test status:

| Test ID | Scenario | Linux run | Windows run |
|---|---|---|---|
| SOCK_RT_001 | Create registers runtime entry; close removes it | Pass | Pass |
| SOCK_RT_002 | Blocked/unblocked transitions update write interest | Pass | Pass |
| SOCK_RT_003 | Repeated create/close leaves no registration leaks | Pass | Pass |

Compatibility rerun snapshot (`clang-x64`):
- Linux: Stage 1 (`SOCK_BHV_001`-`SOCK_BHV_020`) 20/20 pass; Stage 2 (`SOCK_OP_001`-`SOCK_OP_003`) 3/3 pass; Stage 3 (`SOCK_RT_001`-`SOCK_RT_003`) 3/3 pass.
- Windows: Stage 1 (`SOCK_BHV_001`-`SOCK_BHV_020`) 20/20 pass; Stage 2 (`SOCK_OP_001`-`SOCK_OP_003`) 3/3 pass; Stage 3 (`SOCK_RT_001`-`SOCK_RT_003`) 3/3 pass.

Runtime diagnostics update:
- `dump_socket_status()` now includes a Socket Runtime Diagnostics section (registration state, tracked fd, event mask, context presence, stale mapping hint) to support Stage 3/4 operational debugging.

Stage 3 gate:
- [x] Stage complete when runtime lifecycle is leak-free and baseline semantics are preserved.

### Stage 4 Checklist: Async DNS with Capacity Lockdown

Stage 4 is split into two delivery tracks:
- Driver DNS track (optional at build time): hostname support in socket connect path via async worker resolution.
- Mudlib DNS track (always viable): DNS queries implemented in mudlib using socket efuns and numeric address connect.

#### Stage 4A Checklist: Driver DNS Track (optional build feature)

- [x] Harden socket address parsing first: reject malformed non-numeric host tokens unless DNS track is enabled.
- [x] Reuse existing build-time feature option in `options.h` (`PACKAGE_PEER_REVERSE_DNS`) to gate built-in socket-connect DNS resolution.
- [x] Add DNS worker pool and completion posting via `async_runtime_post_completion()`.
- [x] Add global in-flight DNS cap (64).
- [x] Add bounded pending DNS queue (256 entries, DROP_OLDEST).
- [x] Add per-owner DNS cap (8 per owner).
- [x] Add optional duplicate lookup coalescing.
- [x] Add DNS-phase timeout and total operation deadline handling.
- [x] Add deterministic overload/timeout error mapping via operation-ID correlation.
- [x] Add DNS telemetry counters (admitted/rejected/timed-out/dedup-hit).
- [x] Add flood tests for global/per-owner limits.
- [x] Confirm backend loop remains responsive under DNS timeout and flood scenarios.

#### Stage 4B Checklist: Mudlib DNS Track (always available)

- [ ] Document mudlib resolver flow using DATAGRAM socket efuns (query, parse, callback, timeout).
- [ ] Add mudlib-side timeout/retry guidance and deterministic failure mapping guidance.
- [ ] Add interop guidance for using numeric address + port with `socket_connect` after mudlib resolution.
- [ ] Add compatibility note for deployments with built-in DNS disabled.

Stage 4 DNS-disabled build acceptance criteria:
- [x] Numeric address connect behavior remains unchanged (`socket_connect` with dotted IPv4 + port).
- [x] Hostname connect attempts fail fast and deterministically with `EEBADADDR` when built-in DNS is disabled.
- [x] Stage 1-3 suites remain green with DNS disabled.

### Stage 4 Verification Status (2026-03-22, Complete)

Implementation source:
- `lib/socket/socket_efuns.c` — operation-ID correlation in DNS task/result pipeline, stale completion guard, dedup leader tracking.
- `lib/socket/socket_efuns.h` — exported DNS telemetry and completion handler.
- `src/comm.c` — DNS_COMPLETION_KEY event routing to main loop.
- `tests/test_socket_efuns/test_socket_efuns_behavior.cpp` — 6 targeted DNS test cases.

Current Stage 4 targeted test status (full suite executed):

| Test ID | Scenario | Linux run | Windows run |
|---|---|---|---|
| SOCK_DNS_003 | DNS-enabled build resolves hostname (`localhost <port>`) and reaches transfer/success path | Pass (549ms) | Pass |
| SOCK_DNS_004 | Global DNS admission cap rejects requests beyond 64 in-flight lookups | Pass (156ms) | Pass |
| SOCK_DNS_005 | Per-owner DNS admission cap rejects requests beyond 8 in-flight lookups | Pass (108ms) | Pass |
| SOCK_DNS_006 | DNS timeout before total operation deadline maps deterministically to TIMED_OUT phase | Pass (188ms) | Pass |
| SOCK_DNS_011 | Backend responsiveness under DNS flood (numeric connect path while DNS work in-flight) | Pass (243ms) | Pass |
| SOCK_DNS_012 | Duplicate hostname lookups coalesce; followers complete when leader result arrives | Pass (136ms) | Pass |

Execution notes:
- 6/6 tests pass deterministically on both Linux and Windows `clang-x64` presets (minor fixes applied).
- Operation-ID correlation prevents stale completion cross-contamination between test boundaries.
- Dedup leader op_id tracking ensures followers route to correct leader operation.
- Timeout forced-completion via test hook validates deterministic phase transition to OP_TIMED_OUT.
- Telemetry counters (`admitted`, `dedup_hit`, `timed_out`) validate flow through correct code paths.

DNS-disabled build validation:
- `SOCK_DNS_001` and `SOCK_DNS_002` pass with feature disabled; numeric connect unchanged, hostname connect rejects with deterministic `EEBADADDR`.
- Stage 1-3 suites (`SOCK_BHV_*`, `SOCK_OP_*`, `SOCK_RT_*`) remain green (26/26) with DNS disabled.

### Stage 4 Verification Matrix (Stage 4A/4B)

Use these IDs directly in test names once implementation begins.

| Test ID | Track | Scenario | Expected result |
|---|---|---|---|
| SOCK_DNS_001 | 4A | DNS build feature disabled; `socket_connect` with dotted IPv4 + port | `EESUCCESS`/existing success path semantics unchanged |
| SOCK_DNS_002 | 4A | DNS build feature disabled; `socket_connect` with hostname + port | deterministic `EEBADADDR`; no operation table leak |
| SOCK_DNS_003 | 4A | DNS build feature enabled; hostname resolution success | connect transitions through DNS phase and reaches transfer/success path |
| SOCK_DNS_004 | 4A | Flood hostname connects beyond global DNS cap | deterministic overload mapping; backend loop remains responsive |
| SOCK_DNS_005 | 4A | Per-owner DNS cap exceeded | deterministic owner-cap rejection mapping |
| SOCK_DNS_006 | 4A | DNS timeout before total operation deadline | deterministic timeout mapping; exactly one terminal completion |
| SOCK_DNS_007 | 4A | Duplicate hostname lookups with coalescing enabled | dedup-hit telemetry increments; all waiters complete deterministically |
| SOCK_DNS_008 | 4B | Mudlib UDP resolver query + response parse | callback receives resolved numeric address |
| SOCK_DNS_009 | 4B | Mudlib resolver timeout/retry exhaustion | deterministic mudlib failure callback; no backend stall |
| SOCK_DNS_010 | 4B | Mudlib resolver output used for numeric `socket_connect` | connect behavior matches numeric baseline |

Stage 4 gate:
- [x] Stage complete when selected DNS track(s) are non-blocking, bounded, and stable under adversarial load, with explicit DNS-disabled build behavior verified.

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

**Goal**: provide non-blocking DNS under bounded resource controls with explicit support for DNS-disabled builds.

Delivery tracks:
- Track A (driver DNS, optional build feature): hostname resolution in socket connect path.
- Track B (mudlib DNS): resolver implemented at mudlib layer using socket efuns.

Tasks:
- Track A:
  - Add parser hardening for malformed/non-numeric host tokens.
  - Add build-time switch for built-in DNS support in socket connect path.
  - Implement DNS worker pool and completion posting to `async_runtime`.
  - Add admission-control policy:
    - global in-flight cap
    - bounded pending queue
    - per-owner cap
    - optional duplicate lookup coalescing
  - Add timeout handling for DNS phase and total operation deadline.
  - Map overload and timeout to deterministic socket errors.
- Track B:
  - Define mudlib resolver contract (query, callback, timeout, retry).
  - Validate numeric connect interop after mudlib resolution.

Required protections:
- Track A:
  - Reject over-capacity requests immediately.
  - Never allow unbounded queue growth.
  - Collect counters: admitted, rejected-global, rejected-owner, timed-out, dedup-hit.
- DNS-disabled builds:
  - Numeric connect remains baseline-compatible.
  - Hostname connect fails deterministically with `EEBADADDR`.

Exit criteria:
- DNS timeout scenarios do not stall backend loop.
- Flooding attempts remain within configured limits.
- DNS-disabled build behavior is validated.
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

