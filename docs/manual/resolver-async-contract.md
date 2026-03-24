# Resolver Async Contract (Stage 5)

**Status**: Stage 5 finalized (2026-03-24)  
**Audience**: Operators and mudlib maintainers  
**Scope**: Runtime behavior for driver-managed DNS in `socket_connect`, `resolve()`, and `query_ip_name()`.

## Summary

Stage 5 moves DNS resolution to the shared async resolver and removes legacy blocking resolver paths from runtime execution.

- All DNS work is queued and processed off the main backend thread.
- Behavior is parity-verified with and without c-ares.
- Forward and reverse resolver caches are centralized in `src/addr_resolver.cpp`.
- Admission control and telemetry are centralized in resolver core.

For developer-facing architecture and subsystem boundaries, see [../internals/addr-resolver.md](../internals/addr-resolver.md).

## Backend Selection

Build-time behavior:

- `HAVE_CARES=1`: c-ares backend is used.
- `HAVE_CARES` undefined: fallback worker pool is used around libc resolver calls.

In both modes:

- Main thread does not call blocking DNS APIs.
- Request completion returns through async resolver result queue.

## Efun and Socket Behavior

### `socket_connect(fd, "hostname port", read_cb, write_cb)`

- Hostname arguments are supported in all Stage 5 builds.
- Driver checks forward cache before enqueueing DNS work.
- On forward-cache hit, connect proceeds immediately on cached numeric address.
- On cache miss, request is admitted through shared resolver policy and completes asynchronously.
- Duplicate in-flight hostname lookups may be coalesced.

### `resolve(name, callback)` (`query_addr_number` internal path)

- Forward cache is checked first for non-numeric hostnames.
- Cache hit triggers immediate callback with cached numeric IP.
- Cache miss follows async enqueue and completion callback flow.
- Timeout/failure path delivers undefined result through callback contract.

### `query_ip_name(object)`

- Returns cached reverse name immediately when available.
- On cache miss, returns numeric IP immediately and schedules background refresh.
- On TTL-expired entries, refresh is scheduled while preserving immediate return behavior.
- Scheduling failures are silent for return-path stability.

## Admission and Capacity Policy

Shared resolver policy is centralized and runtime-configurable.

Defaults:

- Global in-flight cap: `64`
- Forward quota: `10`
- Reverse quota: `4`
- Refresh quota: `2`
- Queue bound: `256`

Saturation behavior:

- Mudlib-initiated requests reject with `EERESOLVERBUSY`.
- Driver-priority forward requests may evict oldest queued task when required by policy.

## Cache and TTL Policy

Resolver-managed caches:

- Forward cache: hostname -> IPv4 (includes negative cache entries)
- Reverse cache: IPv4 -> hostname

Runtime-configurable TTL controls:

- `ResolverForwardCacheTtl`
- `ResolverReverseCacheTtl`
- `ResolverNegativeCacheTtl`
- `ResolverStaleRefreshWindow`

Practical note:

- Resolver cache TTL is driver policy.
- OS-level resolver behavior may also cache results under the fallback backend.
- c-ares path remains under resolver-managed policy for request admission and completion handling.

## Deterministic Completion Model

Each resolver request reaches exactly one terminal outcome:

- success
- timeout
- canceled
- failure/rejection

Protection mechanisms:

- request-id correlation for completions
- safe release of pending callback slots
- stale completion fan-out prevention

## Tracing and Runtime Inspection

### Trace Tier

Stage 5 resolver/cache tracing uses `TT_COMM|3`.

Examples:

- `socket_connect` forward cache hit/miss
- `query_addr_number` forward cache hit/miss
- `query_ip_name` reverse cache hit/miss

Command-line usage (octal flags):

```sh
neolith -f src/neolith.conf -t 0203
```

`0203` enables `TT_COMM` with verbose level 3.

### `dump_socket_status()` Telemetry

Socket status output includes shared resolver telemetry and cache counters:

- in-flight totals by class
- admitted/rejected/dropped counters by class
- dedup hit count
- completed/failed/timed_out counters
- forward cache: hit/miss/negative-hit
- reverse cache: hit/miss

## Validation Snapshot

As of Stage 5 finalization:

- Resolver matrix green with and without c-ares.
- `test_socket_efuns` resolver coverage: 25/25 passing.
- Non-blocking enqueue verified by `RESOLVER_NOBLOCK_001`.
- Forward cache bypass verified by `RESOLVER_CACHE_001`.

## Operational Guidance

Use driver-managed hostname connect by default:

- Prefer `socket_connect(fd, "hostname port", ...)` over custom resolver scaffolding unless mudlib-specific DNS policy is required.
- Keep resolver quotas and TTL values tuned to workload profile.
- Enable `TT_COMM|3` briefly during incident analysis; disable when not needed.
- Use `dump_socket_status()` to confirm cache effectiveness and admission pressure.

For mudlib-managed DNS flows (Stage 4B), see [lpc-dns-resolver.md](lpc-dns-resolver.md).
