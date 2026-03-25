# Shared Resolver Module (`addr_resolver`)

**Status**: Current implementation reference (2026-03-26)  
**Audience**: Driver developers and maintainers  
**Scope**: Internal architecture, module boundaries, invariants, and operational internals for the shared resolver module.

## Purpose

This document records the implemented architecture and behavior of the shared built-in resolver module.  
It is the developer-facing counterpart to operator guidance in [../manual/dns-resolver.md](../manual/dns-resolver.md).

## Architecture

### Module Ownership

- Resolver core, queues, admission policy, and DNS caches are owned by `src/addr_resolver.cpp`.
- Public C API is exposed through `src/addr_resolver.h`.
- Socket efuns consume resolver APIs from `lib/socket/socket_efuns.c`.
- Efun callback path consumes resolver APIs via `src/comm.c` (`query_addr_number` and resolver completion handling).

### Removed/Retired Runtime Paths

- Legacy runtime dependence on `addr_server_fd` is removed.
- Main-thread blocking DNS path is retired.
- Resolver bookkeeping no longer lives in socket-local ad hoc code paths.

## Backends and Selection

Build-time backend selection:

- `HAVE_CARES=1`: c-ares worker path.
- `HAVE_CARES` undefined: fallback worker pool around libc resolver calls.

Both backends share:

- task queue and result queue model
- request-id completion correlation
- admission control and telemetry accounting
- same public resolver APIs and completion semantics

## Public Resolver API Surface

Key APIs used by runtime paths:

- enqueue/dequeue:
  - `addr_resolver_enqueue_lookup`
  - `addr_resolver_enqueue_reverse`
  - `addr_resolver_enqueue_refresh`
  - `addr_resolver_dequeue_result`
- callback reservation lifecycle:
  - `addr_resolver_reserve_lookup_request`
  - `addr_resolver_get_lookup_request`
  - `addr_resolver_release_lookup_request`
- cache APIs:
  - `addr_resolver_forward_cache_get`
  - `addr_resolver_forward_cache_add`
  - `addr_resolver_reverse_cache_get`
  - `addr_resolver_reverse_cache_add`
  - `addr_resolver_cache_reset`
- telemetry/config:
  - `addr_resolver_get_telemetry`
  - `addr_resolver_get_config`
  - `addr_resolver_config_init_defaults`

## Runtime Behavior by Entry Point

### `socket_connect` Hostname Path

Implemented behavior:

1. Parse `"hostname port"` endpoint.
2. Forward-cache check first.
3. Cache hit: populate remote sockaddr immediately and skip resolver enqueue.
4. Cache miss: enqueue resolver request through centralized policy and transition through DNS phase.

This path uses `TT_COMM|3` trace messages for cache hit/miss visibility.

### `resolve` Callback Path (`query_addr_number`)

Implemented behavior:

1. Non-numeric query checks forward cache first.
2. Cache hit: invoke callback immediately with cached numeric IP (no async request reservation).
3. Cache miss: reserve lookup request and enqueue async lookup.
4. Completion path applies callback with request-id validation and safe release.

### `query_ip_name`

Implemented behavior:

1. Reverse-cache check first.
2. Cache hit: immediate hostname return.
3. Cache miss: immediate numeric IP return plus fire-and-forget reverse refresh enqueue.
4. TTL-expired entries are treated as miss for return semantics while refresh updates in background.

This path also emits `TT_COMM|3` hit/miss traces.

## Cache Design and Semantics

### Forward Cache

- Key: hostname string
- Value: IPv4 numeric address
- Supports negative cache entries for failed lookups
- TTL controls:
  - `ResolverForwardCacheTtl`
  - `ResolverNegativeCacheTtl`

### Reverse Cache

- Key: IPv4 address (network order)
- Value: hostname string
- TTL control:
  - `ResolverReverseCacheTtl`
- Refresh-related policy:
  - `ResolverStaleRefreshWindow`

### Ownership Rule

All cache mutations and lifecycle operations flow through resolver API only.  
Socket and comm layers must not own duplicate resolver caches.

## Admission Policy and Quotas

Centralized policy is enforced in resolver enqueue paths.

Defaults:

- global in-flight cap: `64`
- forward quota: `10`
- reverse quota: `4`
- refresh quota: `2`
- queue bound: `256`

Behavioral notes:

- mudlib-initiated requests reject on saturation
- driver-priority forward requests may evict oldest queued item under policy
- coalescing/dedup accounting is centralized

## Telemetry and Diagnostics

Resolver telemetry includes:

- in-flight/admitted/rejected/dropped by class
- dedup hit
- completed/failed/timed_out
- cache counters:
  - forward hit
  - forward miss
  - forward negative-hit
  - reverse hit
  - reverse miss

These are surfaced by socket diagnostics (`dump_socket_status`) and aid production triage.

## Invariants

Resolver module invariants:

1. DNS calls do not block the main backend thread.
2. Each request reaches exactly one terminal outcome.
3. Stale completion fan-out is prevented by request-id correlation.
4. Cache ownership remains centralized in resolver module.
5. Resolver behavior is parity-verified across backend variants.

## Test and Validation Snapshot

Current status:

- resolver-focused suite is green across with/without-c-ares configurations
- `RESOLVER_NOBLOCK_001` validates enqueue non-blocking behavior
- `RESOLVER_CACHE_001` validates forward-cache bypass behavior in socket path
