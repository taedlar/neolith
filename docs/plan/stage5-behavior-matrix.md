# Stage 5: Shared Resolver Behavior Matrix

**Purpose**: Document expected runtime behavior of the shared resolver across build variants and DNS operation classes.

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

## Implementation Checklist (By Build Variant)

### WITH c-ares (HAVE_CARES defined)
- [ ] Verify c-ares is discoverable and linked.
- [ ] Add c-ares DNS task queue and worker pool.
- [ ] Implement forward-lookup request class (hostname → IP).
- [ ] Implement reverse-lookup request class (IP → hostname).
- [ ] Add admission control gates (global 64, per-class caps).
- [ ] Add coalescing by hostname (socket connect optimization).
- [ ] Test all five operation classes under c-ares backend.
- [ ] Verify no main-thread blocking in trace output.

### WITHOUT c-ares (HAVE_CARES undefined)
- [ ] Implement fallback resolver worker using standard getaddrinfo.
- [ ] Route fallback resolver through SAME async worker pool pattern as c-ares.
- [ ] Verify getaddrinfo calls only in worker threads (not main).
- [ ] Run all five operation classes through fallback backend.
- [ ] Verify non-blocking invariant holds (no stalls in main loop trace).
- [ ] Document latency trade-off (higher without c-ares, but still responsive).
- [ ] Build and test without c-ares variant; ensure all tests pass.

### Both Builds
- [ ] Dedup coalescing for socket connect forward lookups (primary volume case).
- [ ] Deterministic timeout and cancellation semantics.
- [ ] No stale completion fan-out (request-id correlation verified).
- [ ] Admission control rejection mapping to caller errors.
- [ ] Legacy `addr_server_fd` path disabled (no fallback to blocking behavior).

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

## Conclusion

Stage 5 enables a non-blocking shared resolver with three key properties:

1. **Non-blocking by design**: all DNS work in worker threads; main loop never blocks.
2. **Adaptive backend**: with c-ares for production performance, fallback for compatibility.
3. **Deterministic completion**: every request reaches exactly one terminal state (success, timeout, cancel, error).

The behavior matrix ensures that operators and LPC developers understand what changes between builds and across operation families, enabling informed deployment decisions.
