# Stage 5: Shared Resolver Behavior Matrix

**Purpose**: Document expected runtime behavior of the shared resolver across build variants and DNS operation classes.

**Context**: This document is the authoritative specification for Stage 5 of the [Socket Operation Engine roadmap](socket-operation-engine.md#milestone-5-shared-resolver-migration-c-ares). It includes behavior specifications, implementation status, test tracking, and next-session priorities.

**Dimensions**:
1. **Build-time**: `HAVE_CARES` defined vs undefined
2. **DNS Operations**: five distinct families
3. **Execution guarantees**: non-blocking, admission control, deterministic completion

---

## DNS Operation Classes

### Class A: Socket Connect Forward Lookup (Mandatory)
**Trigger**: `socket_connect(fd, "hostname <port>", read_cb, write_cb)` or `socket_connect(fd, "IP <port>", read_cb, write_cb)`  
**Note**: Hostname support is mandatory in all Neolith builds. The build-time decision in Stage 5 is the resolver backend (`HAVE_CARES` vs fallback), not the availability of hostname parsing itself.  
**Operation**: resolve hostname to IPv4/IPv6 address before TCP handshake.  
**Caller Contract**:
- Non-blocking; completion fires via socket write-select or timeout.
- Deterministic terminal state: `EESUCCESS`, `EEBADADDR` (malformed), `EETIMEOUT`, `EECANCELED` (on close).
- Admission: global cap 64, per-owner cap 8, queue limit 256 (DROP_OLDEST).

---

### Class B: Manual Forward Lookup (resolve() efun)
**Stage 5 migration status**: legacy path currently active; target is shared resolver cutover.  
**Trigger**: `resolve("hostname")` in LPC  
**Operation**: return IP address(es) for hostname.  
**Caller Contract**:
- Current behavior: may block getaddrinfo() on legacy backend.
- Stage 5 contract: must not block main loop; return cached result or fire async completion callback.
- Admission: global cap 64, per-resolver-call cap 1 (no batch semantics), queue limit 256.

---

### Class C: Auto Reverse Lookup (query_ip_name cache population)
**Stage 5 migration status**: legacy `addr_server_fd` path currently active; target is shared resolver cutover.  
**Trigger**: User connection → driver auto-calls `get_player_ip()` / internal IP cache refresh.  
**Operation**: resolve IP address back to hostname (optional).  
**Caller Contract**:
- Non-blocking; completion feeds into internal IP→hostname cache.
- Deterministic: success (hostname cached) or timeout (remains IP).
- Admission: global cap 64, per-user-session cap 2, queue limit 256.

---

### Class D: Manual Reverse Lookup (query_ip_name() efun)
**Stage 5 migration status**: legacy path currently active; target is shared resolver cutover.  
**Trigger**: `query_ip_name(ip_string)` in LPC  
**Operation**: return cached hostname or trigger reverse lookup.  
**Caller Contract**:
- Current: may block getaddrinfo() on legacy backend.
- Stage 5 contract: non-blocking; return cached result or fire async completion callback.
- Admission: global cap 64, per-query cap 1, queue limit 256.

---

### Class E: Peer Name Refresh (query_ip_name session updates)
**Stage 5 migration status**: internal legacy refresh path currently active; target is shared resolver cutover.  
**Trigger**: Principal user descriptor session event (e.g., heartbeat).  
**Operation**: refresh hostname cache for active user connections.  
**Caller Contract**:
- Non-blocking; completion feeds cache.
- Deterministic; may co-queue with manual queries on same IP.
- Admission: global cap 64, per-session cap 1, queue limit 256.

---

## Behavior Matrix: WITH c-ares (HAVE_CARES defined)

| Class | Operation | Backend | Blocking | Completion | Admission | Error Handling | Notes |
|-------|-----------|---------|----------|-----------|-----------|----------------|-------|
| **A** | Socket connect forward | c-ares worker pool (DNS task queue) | non-blocking | socket write-select or timeout | global 64, owner 8, q=256 | `EEBADADDR` (malformed address), `EERESOLVERBUSY` (admission reject), `EETIMEOUT` (after delay), `EECANCELED` (on socket close) | hostname detected in parser; port extracted; request enqueued with IP validation before handshake |
| **B** | resolve() | c-ares worker pool | non-blocking | async callback to mudlib (new contract) | global 64, per-call 1, q=256 | admission reject signals `EERESOLVERBUSY`; timeout is deterministic timeout failure | efun must be reimplemented as async (requires mudlib update to consume callback) |
| **C** | Auto reverse on user connect | c-ares worker pool | non-blocking | internal cache entry | global 64, per-session 2, q=256 | timeout → remain IP-only in cache | optional feature; no user-visible latency |
| **D** | query_ip_name() | c-ares worker pool (cached first) | non-blocking | immediate return value | global 64, per-query 1, q=256 | cached hit returns hostname; cache miss returns numeric IP immediately and schedules async refresh; admission reject signals `EERESOLVERBUSY` | preserves legacy fallback style; no nil on cache miss |
| **E** | Peer name refresh (internal) | c-ares worker pool | non-blocking | internal cache entry | global 64, per-session 1, q=256 | timeout → remain previous cache value | internal operation; no mudlib contract change |

**Performance Profile (WITH c-ares)**:
- Typical latency: 10–100 ms per resolve (network + c-ares worker latency).
- Zero main-thread blocking; all latency absorbed by worker pool.
- Coalescing: multiple requests for same hostname wait on single worker result.
- Worst case (queue full): new requests are explicitly rejected with `EERESOLVERBUSY`.

---

## Behavior Matrix: WITHOUT c-ares (HAVE_CARES undefined)

| Class | Operation | Backend | Blocking | Completion | Admission | Error Handling | Notes |
|-------|-----------|---------|----------|-----------|-----------|----------------|-------|
| **A** | Socket connect forward | fallback sync resolver in worker (no c-ares) | non-blocking (worker-mediated) | socket write-select or timeout | global 64, owner 8, q=256 | `EEBADADDR` (malformed address), `EERESOLVERBUSY` (admission reject), `EETIMEOUT` (after delay), `EECANCELED` (on socket close) | fallback uses getaddrinfo-via-worker pattern (no main-thread blocking); latency higher than c-ares |
| **B** | resolve() | fallback sync resolver in worker | non-blocking (worker-mediated) | async callback to mudlib (same contract as WITH c-ares) | global 64, per-call 1, q=256 | admission reject signals `EERESOLVERBUSY`; timeout is deterministic timeout failure | mudlib contract identical to c-ares case (async-only); implementation detail hidden from user |
| **C** | Auto reverse on user connect | fallback sync resolver in worker | non-blocking (worker-mediated) | internal cache entry | global 64, per-session 2, q=256 | timeout → remain IP-only in cache | same semantics as c-ares; latency higher |
| **D** | query_ip_name() | fallback sync resolver in worker (cached first) | non-blocking (worker-mediated) | immediate return value | global 64, per-query 1, q=256 | cached hit returns hostname; cache miss returns numeric IP immediately and schedules async refresh; admission reject signals `EERESOLVERBUSY` | same contract as c-ares case; latency affects refresh timing only |
| **E** | Peer name refresh (internal) | fallback sync resolver in worker | non-blocking (worker-mediated) | internal cache entry | global 64, per-session 1, q=256 | timeout → remain previous cache value | same semantics; higher latency trade-off |

**Performance Profile (WITHOUT c-ares)**:
- Typical latency: 50–500 ms per resolve (getaddrinfo in worker + queue latency).
- Zero main-thread blocking; fallback uses worker-pool pattern identical to c-ares flow.
- Coalescing: identical to c-ares case (multiple requests coalesce on single worker result).
- Worst case (queue full): new requests are explicitly rejected with `EERESOLVERBUSY` (same as c-ares).

---

## Key Guarantees (Both Builds)

### Non-blocking Invariant
- Main event loop never calls blocking DNS functions (getaddrinfo, gethostbyname, reverse DNS).
- All DNS work queues to async worker; completion posts back via `async_runtime_post_completion()`.
- Even WITHOUT c-ares, fallback backend runs in worker thread (not main thread).

### Deterministic Completion
- Each DNS request reaches **exactly one** terminal state: success, timeout, canceled, or error.
- No stale completion fan-out after timeout or cancellation.
- Request-id correlation prevents stale results from reaching wrong owner/socket.

### Admission Control
- Global cap (64 in-flight) prevents unbounded queue growth.
- Per-class caps (per-owner 8 for socket, per-session 2 for background, etc.) prevent starvation.
- Queue size limit (256) with DROP_OLDEST eviction on overflow.
- Explicit rejection (`EERESOLVERBUSY`, `EECANCELED`, `EETIMEOUT`) instead of silent drop.

### Dedup/Coalescing
- Multiple requests for same hostname route to single worker future.
- All waiters complete together; no duplicate queries on network.
- Applies to socket connect forward lookups (high-impact volume case).

---

## Migration Impact: Behavior Changes

### Hostname Support in socket_connect() (Now Mandatory)
**Stage 4A / Stage 5**: Hostname support in `socket_connect(fd, "hostname <port>", ...)` is mandatory in all builds.
- **Motivation**: Mudlib convention of using simul_efun wrappers (resolve() + numeric connect) is no longer viable when resolve() becomes async-only in Stage 5.
- **Consequence**: Mudlibs can now assume hostname parsing is always available; no workarounds needed.
- **No compatibility break**: Existing numeric-IP arguments ("127.0.0.1 8000") continue to work unchanged.

### resolve() Efun (Async Contract Change)
**Current (Legacy blocking path)**:
```c
int resolve(string hostname) {
  // Blocking getaddrinfo call; may stall main loop for 100ms–1s
  return IPv4_address_integer;
}
```

**Stage 5 (Shared resolver async)**:
```c
int resolve(string hostname) {
  // Queue async request; return result or fire completion callback
  // Mudlib must handle async contract: result immediate (cached) or callback-based
}
```

**Incompatibility**: LPC code expecting synchronous return must change to consume callback.  
**Migration path**: 
- Direct usage: Mudlib can now use `socket_connect(fd, hostname, ...)` directly (no workaround needed).
- Legacy resolve() callers: Mudlib must implement async-aware wrapper (state machine or continuation-passing style) if synchronous return is required.

### query_ip_name() Efun
**Current**:
```c
string query_ip_name(string ip) {
  // May be cached (fast) or trigger blocking reverse lookup
  return cached_or_looked_up_hostname;
}
```

**Stage 5**:
```c
string query_ip_name(string ip) {
  // Return cached hostname when available.
  // On cache miss, return numeric IP immediately and schedule async reverse refresh.
}
```

**Compatibility impact**: No nil-on-miss contract is introduced; callers continue to receive a string immediately.  
**Mitigation**: Cache is populated/refreshed by background reverse-lookup worker; typical cache hit rate improves over steady-state traffic.

---

## Rejection and Error Mapping

### Admission Control Rejection
When queue is full or global cap exceeded:
- **Socket connect**: return `EERESOLVERBUSY`.
- **resolve()**: surface explicit resolver admission failure `EERESOLVERBUSY` via resolver completion/error contract.
- **query_ip_name()**: surface explicit resolver admission failure `EERESOLVERBUSY` via resolver completion/error contract.

### Timeout
- **Socket connect**: `EETIMEOUT` (to caller's `write_cb` or operation completion).
- **resolve()**: exception "resolver timeout".
- **query_ip_name()**: exception "resolver timeout"; cache remains unchanged.

### Cancellation
- **Socket connect**: `EECANCELED` if socket closed before DNS completion.
- **resolve()**: exception "resolver canceled" if owner object destructed.
- **query_ip_name()**: exception "resolver canceled" if owner object destructed.

---

## No Blocking Paths (Critical Constraint)

The following MUST NOT occur in Stage 5:
- ❌ No `getaddrinfo()` call on main thread.
- ❌ No `gethostbyname()` call on main thread.
- ❌ No `reverse_dns()` call on main thread.
- ❌ No legacy `addr_server_fd` event handling.
- ❌ No `query_addr_number()` internal path.
- ❌ No blocking fallback if c-ares unavailable; fallback uses worker pool instead.

---

## Stage 5 Unified Checklist

- [x] Verify c-ares is discoverable and linked (`FindCARES.cmake`, `HAVE_CARES`, stem link).
- [x] Add c-ares DNS task queue and worker pool (`cares_worker_main` in `addr_resolver.cpp`).
- [x] Implement shared forward-lookup and reverse-lookup request classes.
- [x] Implement fallback resolver worker using standard getaddrinfo.
- [x] Route fallback resolver through the same async worker pool pattern as c-ares.
- [x] Verify getaddrinfo calls only in worker threads (not main).
- [ ] Complete shared resolver cache migration: add driver-owned forward/reverse TTL cache and keep `query_ip_name()` stable while cache ownership moves out of `src/comm.c`.
- [x] Define runtime-configurable resolver policy settings, pass them through `addr_resolver_init()` via a resolver settings struct, and source them from stem/runtime startup.
- [ ] Run all five operation classes through fallback backend.
- [ ] Verify non-blocking invariant holds (no stalls in main loop trace).
- [ ] Document latency trade-off (higher without c-ares, but still responsive).
- [x] Build and test without c-ares variant.
- [ ] Add admission control gates (global + per-class caps).
- [ ] Add admission control rejection mapping to caller-visible errors.
- [x] Add dedup/coalescing for socket connect forward lookups (primary volume case).
- [ ] Verify deterministic timeout and cancellation semantics across all classes.
- [ ] Add resolver telemetry and verification for per-class lifecycle counters plus forward/reverse cache hit, miss, stale-hit, and negative-hit behavior.
- [x] Verify no stale completion fan-out (request-id correlation).
- [x] Disable legacy `addr_server_fd` runtime path and legacy hname bridge.
- [ ] Test all five operation classes under c-ares backend.
- [ ] Verify no main-thread blocking in trace output for c-ares backend.
- [x] Route socket-connect DNS path through the same shared resolver request classes used by `resolve()` and reverse-refresh.
- [ ] Document c-ares cache usage and OS resolver cache assumptions relative to shared resolver TTL policy.
- [ ] Publish operator-facing behavior deltas for `resolve()` and `query_ip_name()` under Stage 5 async contract.

## Stage 5 Unified Verification Status (2026-03-24, c-ares backend + CLASS A-C tests complete)

**Backend Implementation:**
- [x] `resolve()` requests run through shared resolver queue/results flow in `src/addr_resolver.cpp` with request-id correlation and safe pending-request release.
- [x] `query_ip_name()` cache misses enqueue reverse-lookup refresh and still return numeric IP immediately.
- [x] Legacy `addr_server_fd` event branch and hname parser bridge removed from runtime dispatch.
- [x] Legacy resolve() request bookkeeping removed from `src/comm.c`; resolver bookkeeping now owned by shared resolver module.
- [x] Resolver runtime policy settings now come from runtime config and are passed through stem into all shared resolver init paths via `addr_resolver_init()` config struct.
- [x] Reverse-name efun cache remains intentionally in `src/comm.c` (`ip_name_cache`) pending OS-cache policy decisions.
- [x] No-c-ares build verified and tests executed without observed resolver regression failures in captured run output.
- [x] `socket_connect()` hostname DNS path now runs through shared resolver request-id completions instead of a socket-local DNS worker path.
- [x] Focused no-c-ares fallback concurrency coverage confirms independent progress when one blocking lookup is delayed.
- [x] c-ares backend (`cares_worker_main`) added to `addr_resolver.cpp` behind `#ifdef HAVE_CARES`; uses `ares_init`/`ares_gethostbyname`/`ares_getnameinfo`/`ares_destroy` per task (channel-per-task pattern); event loop driven by `select()` + `ares_process()`.
- [x] `ares_library_init`/`ares_library_cleanup` called at `addr_resolver_init`/`addr_resolver_deinit` from main thread.
- [x] Build with `HAVE_CARES` (Ubuntu `libc-ares-dev`) succeeds; 174/174 tests pass on Linux Debug config.

## Stage 5 Completion Summary (2026-03-24)

**What's Complete:**
- [x] c-ares backend is live under `HAVE_CARES`; fallback (getaddrinfo-in-worker) still active without c-ares.
- [x] Both paths use the same task/result queues, completion key, and public API.
- [x] **CLASS A tests (10/10 passing)**: Socket connect forward lookup parity verified for c-ares and fallback backends.
- [x] **CLASS B tests (5/5 passing)**: resolve() efun scaffolds with async callback infrastructure tested.
- [x] **CLASS C tests (6/6 passing)**: Auto-reverse cache population via interactive object tested (3 c-ares + 3 fallback).
- [x] Parity verification tests for c-ares path demonstrate identical behavior to fallback path across timeout, dedup, and admission scenarios.

**What's Pending:**
- [ ] **CLASS D tests (0/10)**: query_ip_name() manual reverse lookup (requires LPC function call testing).
- [ ] **CLASS E tests (0/3)**: Peer name refresh / background heartbeat triggering.
- [ ] Class-aware admission controls and per-class telemetry counters (currently only global + socket-owner caps from Stage 4).
- [ ] Shared forward/reverse TTL cache and `query_ip_name()` cache ownership migration; current 64-entry round-robin cache stays in `src/comm.c` for now.
- [ ] Assertion strengthening for Classes B/C (replace scaffolds/telemetry checks with final async contract assertions once callback semantics finalized).
- [ ] Windows c-ares support validation: may require `ares_getsock()` instead of `ares_fds()` + `select()` if default channel mode doesn't use file descriptors.

## Stage 5 Next-Session Priorities

1. **Implement CLASS D tests (query_ip_name() manual)** — Required before CLASS E; foundational for LPC async testing infrastructure.
2. **Implement CLASS E tests (peer refresh)** — Background heartbeat-triggered refresh and dedup coalescing.
3. **Strengthen B/C assertions** — Replace current scaffolds/telemetry monotonicity checks with final async contract assertions once resolver callback behaviors are finalized.

---

## Testing Strategy

### Matrix Verification Tests
Use operation family × build variant grid:

| | WITH c-ares | WITHOUT c-ares |
|---|---|---|
| **Socket connect forward** | RESOLVER_A_CARES_001 through RESOLVER_A_CARES_005 | RESOLVER_A_NOCARES_001 through RESOLVER_A_NOCARES_005 |
| **resolve() manual** | RESOLVER_B_CARES_001 through RESOLVER_B_CARES_005 | RESOLVER_B_NOCARES_001 through RESOLVER_B_NOCARES_005 |
| **Auto reverse (user connect)** | RESOLVER_C_CARES_001 through RESOLVER_C_CARES_003 | RESOLVER_C_NOCARES_001 through RESOLVER_C_NOCARES_003 |
| **query_ip_name() manual** | RESOLVER_D_CARES_001 through RESOLVER_D_CARES_005 | RESOLVER_D_NOCARES_001 through RESOLVER_D_NOCARES_005 |
| **Peer name refresh (internal)** | RESOLVER_E_CARES_001 through RESOLVER_E_CARES_003 | RESOLVER_E_NOCARES_001 through RESOLVER_E_NOCARES_003 |

**Subtests per case**:
1. Basic success path (hostname resolves).
2. Cache hit (repeat request; verify no queue).
3. Timeout (configure short TTL; verify timeout error).
4. Admission control overflow (queue full; verify rejection).
5. Owner/object destruction during pending request (verify safe cleanup, no callback to freed object).

---

## Decisions Locked

1. **Fallback resolver backend choice**: use standard `getaddrinfo` in worker threads for non-c-ares builds.
   - Rationale: simplest and most portable fallback while preserving non-blocking main-loop behavior.
   - Trade-off: higher latency without c-ares is accepted.

2. **resolve() and query_ip_name() async contract shape**: use async completion callback pattern.
   - Rationale: consistent with the socket-connect async flow and resolver worker model.
   - Follow-up: documentation and mudlib updates remain required.

3. **query_ip_name() return value on cache miss**: return numeric IP string immediately; schedule async reverse refresh in background.
   - Rationale: preserves legacy non-blocking fallback contract and avoids nil-handling regressions in existing mudlibs.
   - Trade-off: caller receives a stale (numeric) value until the background refresh completes and updates the cache.

---

## Performance Expectations

| Build | Small Hostname | Cached Hostname | Timeout |
|-------|---|---|---|
| **WITH c-ares** | 10–100 ms (c-ares worker latency) | <1 ms (cache hit) | 5–10 s (default timeout) |
| **WITHOUT c-ares** | 50–500 ms (getaddrinfo + worker latency) | <1 ms (cache hit) | 5–10 s (default timeout) |

**Main loop impact**: zero stalls in both cases (all work in worker threads).

---

## Rollout Risk & Mitigation

**Risk: resolve() and query_ip_name() async contract breaks existing LPC code.**
- Mitigation: Publish deprecation notice with timeline; provide mudlib helper for async migration and staged rollout guidance.

**Risk: Without c-ares, fallback resolver is slow (500 ms latency).**
- Mitigation: Document performance expectations; recommend c-ares for production; default build includes c-ares support.

**Risk: Admission control rejects legitimate requests under peak load.**
- Mitigation: Tune caps (global 64, per-owner 8) based on mudlib telemetry; add observability counters; publish tuning guide for operators.

**Risk: Legacy addr_server_fd path had specific behavior (e.g., timeout values) that changes.**
- Mitigation: Preserve timeout contract; document any behavior deltas before Stage 5 completion; provide migration guide.

---

## Parity Test Progress (2026-03-23)

Parity verification tests for c-ares backend are being added to validate that the c-ares resolver produces identical behavior to the fallback path.

### Test Coverage Status

**CLASS A: Socket connect forward lookup (COMPLETE)**

**C-ares backend tests:**
- [x] RESOLVER_A_CARES_001: Basic success path (hostname resolves to IP)
- [x] RESOLVER_A_CARES_002: Cache hit / dedup coalescing (repeat hostname, verify dedup counter)
- [x] RESOLVER_A_CARES_003: Timeout with forced DNS timeout hook (verify timed_out telemetry)
- [x] RESOLVER_A_CARES_004: Admission control under load (verify robustness without crashes)
- [x] RESOLVER_A_CARES_005: Owner destruction during pending (verify safe cleanup, no stale callbacks)

**Fallback/NOCARES backend tests:**
- [x] RESOLVER_A_NOCARES_001: Basic success path (hostname resolves to IP)
- [x] RESOLVER_A_NOCARES_002: Cache hit / dedup coalescing (repeat hostname, verify dedup counter)
- [x] RESOLVER_A_NOCARES_003: Timeout with forced DNS timeout hook (verify timed_out telemetry)
- [x] RESOLVER_A_NOCARES_004: Admission control under load (verify robustness without crashes)
- [x] RESOLVER_A_NOCARES_005: Owner destruction during pending (verify safe cleanup, no stale callbacks)

- **Status**: 10/10 tests implemented and passing

**CLASS B: resolve() manual lookup (c-ares)**
- [x] RESOLVER_B_001: Basic success path scaffold for resolve() call surface
- [x] RESOLVER_B_002: Cache/dedup behavior scaffold
- [x] RESOLVER_B_003: Timeout behavior scaffold with forced timeout hook
- [x] RESOLVER_B_004: Admission/load behavior scaffold
- [x] RESOLVER_B_005: Caller destruction cleanup scaffold
- **Status**: 5/5 tests implemented and passing (contract scaffolds)

**CLASS C: Auto reverse on user connect (c-ares)**
- [x] RESOLVER_C_CARES_001: Basic success path via interactive object reverse-refresh trigger
- [x] RESOLVER_C_CARES_002: Repeat lookup/cache path with telemetry monotonicity assertions
- [x] RESOLVER_C_CARES_003: Timeout path remains non-blocking with numeric fallback

**CLASS C: Auto reverse on user connect (fallback/no-c-ares)**
- [x] RESOLVER_C_NOCARES_001: Basic success path via interactive object reverse-refresh trigger
- [x] RESOLVER_C_NOCARES_002: Repeat lookup/cache path with telemetry monotonicity assertions
- [x] RESOLVER_C_NOCARES_003: Timeout path remains non-blocking with numeric fallback
- **Status**: 6/6 tests implemented and passing (3 CARES + 3 NOCARES)

**CLASS D: query_ip_name() manual (c-ares)**
- [ ] RESOLVER_D_CARES_001: Basic success (cached IP returns hostname immediately)
- [ ] RESOLVER_D_CARES_002: Cache hit (repeat query verifies cache)
- [ ] RESOLVER_D_CARES_003: Timeout (reverse DNS timeout, verify numeric IP fallback)
- [ ] RESOLVER_D_CARES_004: Admission control (queue full behavior)
- [ ] RESOLVER_D_CARES_005: Caller destruction during pending (verify safe cleanup)
- **Status**: 0/5 tests implemented (pending - requires LPC function call testing infrastructure)

**CLASS E: Peer name refresh (c-ares)**
- [ ] RESOLVER_E_CARES_001: Background refresh (active user connection triggers refresh)
- [ ] RESOLVER_E_CARES_002: Coalescing (multiple sessions refresh same IP, dedup)
- [ ] RESOLVER_E_CARES_003: Timeout (background refresh timeout, preserve previous cache value)
- **Status**: 0/3 tests implemented (pending - requires background heartbeat simulation)

### Test Infrastructure

**Available utilities**:
- Resolver parity file: [tests/test_socket_efuns/test_socket_efuns_resolver.cpp](tests/test_socket_efuns/test_socket_efuns_resolver.cpp)
- Core socket behavior file: [tests/test_socket_efuns/test_socket_efuns_behavior.cpp](tests/test_socket_efuns/test_socket_efuns_behavior.cpp)

Resolver-focused utilities:
- `handle_dns_completions()`: Poll DNS completion queue
- `WaitForDNSCompletion()`: Block until DNS operation completes
- `get_dns_telemetry_snapshot()`: Inspect admitted/dedup/timed_out counters
- `ScopedDnsTimeoutHook`: Install DNS timeout hook for test scenarios
- `get_socket_operation_info()`: Inspect socket operation state and phase

**Test conventions**:
- Conditional compilation: Tests guarded by `#ifdef HAVE_CARES` to run only when c-ares available
- Telemetry assertions: Verify admission/dedup/timeout counters change as expected
- Timeout testing: Use `ScopedDnsTimeoutHook` to force deterministic timeout
- Load testing: Verify system robustness under concurrent DNS requests without crashes
- Cleanup testing: Verify no stale callbacks after object destruction

### Next Steps

1. **CLASS D (query_ip_name) tests**: Implement cached IP vs. async refresh testing
2. **CLASS E (peer refresh) tests**: Add periodic background refresh triggering
3. **Strengthen Class B/C assertions**: Replace current scaffolds/monotonic checks with final async contract assertions once resolver callback contracts are finalized

---

## Conclusion

Stage 5 enables a non-blocking shared resolver with three key properties:

1. **Non-blocking by design**: all DNS work in worker threads; main loop never blocks.
2. **Adaptive backend**: with c-ares for production performance, fallback for compatibility.
3. **Deterministic completion**: every request reaches exactly one terminal state (success, timeout, cancel, error).

The behavior matrix ensures that operators and LPC developers understand what changes between builds and across operation families, enabling informed deployment decisions.
