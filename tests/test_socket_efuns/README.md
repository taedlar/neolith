# Socket Efun Unit Tests

This directory contains comprehensive unit tests for the socket efun subsystem and the Stage 5 shared resolver backend.

## Test Organization

The test suite is organized into three logically separate files to isolate different testing concerns and reduce build/link complexity:

### 1. **test_socket_efuns_behavior.cpp** — Behavior Lockdown Tests (`SOCK_BHV_*`)

**Purpose**: Validate baseline socket efun behavior and API contracts that must remain stable across refactoring and platform changes.

**Test Families**:
- `SOCK_BHV_001`–`SOCK_BHV_020`: Core socket operation semantics (socket creation, binding, listening, accepting, data transfer, port binding conflicts, error paths, etc.).

**What This Tests**:
- Socket API compliance (e.g., `socket()`, `bind()`, `listen()`, `accept()`, `send()`, `receive()`).
- Error handling and edge cases (malformed arguments, resource exhaustion, invalid states).
- Platform-specific behavior (POSIX vs Windows) is normalized by the driver.

**Key Principle**: These tests must not change unless the underlying socket API contract itself changes. They form the "compatibility baseline" that all other tests assume.

---

### 2. **test_socket_efuns_extensions.cpp** — Operation and Extension Tests (`SOCK_OP_*`, `SOCK_DNS_*`, `SOCK_RT_*`)

**Purpose**: Validate higher-level socket operation features, DNS integration, and runtime behavior that builds on the behavior-lockdown baseline.

**Test Families**:
- `SOCK_OP_*`: Socket operation features (e.g., non-blocking I/O, operation queuing, admission control).
- `SOCK_DNS_*`: DNS subsystem integration (socket-layer DNS callbacks, timeout hooks, telemetry).
- `SOCK_RT_*`: Runtime socket behavior (e.g., interactive descriptor lifecycle, object destruction cleanup).

**What This Tests**:
- Telemetry and observability (admission, dedup, timeout counters).
- DNS timeout forcing via socket-layer hooks (for testing deterministic DNS failures).
- Socket operation lifecycle and state machine progression.
- Integration between socket layer and async runtime.

**Key Principle**: These tests validate the "performance path" and operational guarantees, not API compliance. They may exercise telemetry or test hooks that isolate behavior from production DNS latency.

---

### 3. **test_socket_efuns_resolver.cpp** — Shared Resolver Contract Tests (`RESOLVER_FWD_*`, `RESOLVER_REV_*`, `RESOLVER_REFRESH_*`)

**Purpose**: Validate the Stage 5 shared resolver backend and its three operation classes: Forward Lookup, Reverse Lookup, and Peer Refresh.

**Test Families**:
- `RESOLVER_FWD_001`–`RESOLVER_FWD_010`: Forward hostname→IP resolution (hostname parsing, dedup, admission control, object lifecycle).
- `RESOLVER_REV_001`–`RESOLVER_REV_008`: Reverse IP→hostname lookup for cache refresh (auto refresh on connection, manual refresh on cache miss, timeout behavior, cleanup).
- `RESOLVER_REFRESH_001`–`RESOLVER_REFRESH_003`: Peer refresh for background cache updates (basic enqueue, dedup coalescing, timeout handling).

**What This Tests**:
- Shared resolver public API (`addr_resolver_enqueue_lookup`, `addr_resolver_enqueue_reverse`, `addr_resolver_dequeue_result`).
- Task enqueue/completion flow and request-ID correlation.
- Deterministic behavior under timeout, admission rejection, admission overflow, and object destruction.
- Backend parity: tests pass identically on both `HAVE_CARES` (c-ares worker) and no-c-ares (fallback worker pool) builds.

**Key Principle**: These tests are **backend-agnostic** — they exercise only the public resolver API, not c-ares internals or libc details. This allows a single test suite to validate parity across build variants.

---

## Initialization and Lifecycle Patterns

### Socket Efun Tests: `ScopedTestInteractiveAddr`

For tests that need interactive descriptors (connections with IP addresses):

```cpp
ScopedTestInteractiveAddr interactive(obj, "192.0.2.1");
ASSERT_TRUE(interactive.IsReady()) << "Failed to create test interactive";
// ... test code ...
// Automatically cleaned up when scope exits
```

**What It Does**:
- Creates an interactive descriptor attached to the given object with a fake IP.
- Handles socket creation and binding internally.
- Automatically destroys the descriptor on scope exit.

---

### Resolver Tests: `ScopedAsyncRuntime`

**Critical**: Resolver tests must explicitly initialize the shared resolver, even when running in isolation.

```cpp
ScopedAsyncRuntime runtime_guard;
ASSERT_TRUE(runtime_guard.IsReady()) << "async runtime and resolver are required";
// ... now addr_resolver_enqueue_*() and addr_resolver_dequeue_result() work
```

**What It Does**:
1. Creates the global async runtime (if not already initialized).
2. Fetches resolver configuration from `stem_get_addr_resolver_config()`.
3. Initializes the shared resolver via `addr_resolver_init(g_runtime, &config)`.
4. On destructor: calls `addr_resolver_deinit()` and optionally `async_runtime_deinit()` (only if this test owned the runtime).

**Why This Matters**:
- Running resolver tests in isolation (not through the full driver startup sequence) means the shared resolver won't auto-initialize.
- Without explicit initialization, `addr_resolver_enqueue_reverse()` returns 0 (task queue is null), making tests fail spuriously.
- `ScopedAsyncRuntime` ensures deterministic, reproducible test runs on both platforms.

---

### Resolver Tests: Test Hooks for Determinism

**`ScopedResolverLookupHook`** — Intercept DNS queries for deterministic testing:

```cpp
ScopedResolverLookupHook hook_guard(
  [](const char *query, unsigned int *delay_ms_out, const char **effective_query_out) {
    if (delay_ms_out != nullptr) {
      *delay_ms_out = 0;  // Inject latency if needed
    }
    if (effective_query_out != nullptr && query != nullptr) {
      *effective_query_out = "127.0.0.1";  // Override resolver target
    }
  });
```

**What It Does**:
- Installs a test hook in the resolver worker that intercepts every DNS task.
- `delay_ms_out`: inject milliseconds of processing latency (for timeout forcing).
- `effective_query_out`: replace the query with a different target (e.g., loopback).
- Automatically uninstalls on scope exit.

**Why This Matters**:
- Tests can override resolver behavior without mocking the entire c-ares or libc stack.
- Deterministic: tests don't depend on real DNS latency or network availability.
- Works identically across `HAVE_CARES` and fallback backends (public API, not backend-specific).

**Example — Force Timeout in Refresh Test**:
```cpp
ScopedResolverLookupHook hook_guard(
  [](const char *query, unsigned int *delay_ms_out, const char **effective_query_out) {
    if (delay_ms_out != nullptr && query != nullptr && strcmp(query, "127.0.0.4") == 0) {
      *delay_ms_out = 2100;  // Inject 2.1s delay; deadline is 1s → timeout
    }
  });
ASSERT_EQ(addr_resolver_enqueue_reverse(cache_addr, "127.0.0.4", deadline), 1);
ASSERT_TRUE(WaitForResolverResult(&result, 6000));
EXPECT_TRUE(result.timed_out) << "Injected delay should trigger timeout";
```

---

## Backend Variants: c-ares vs. Fallback

The shared resolver supports two backends, selected at build time:

### WITH c-ares (`HAVE_CARES` defined)

**Backend**: c-ares library (`ares_gethostbyname`, `ares_getnameinfo`), running in a dedicated worker thread.

**Build Command**:
```bash
cmake --preset vs16-x64 -DFETCH_CARES_FROM_SOURCE=v1.34.6
cmake --build --preset ci-vs16-x64
```

**Test Matrix**: Full 21-test resolver suite (10 forward + 8 reverse + 3 refresh) passes on Windows `vs16-x64`.

**Characteristics**:
- Faster DNS resolution (c-ares is optimized for async queries).
- Lower main-thread latency; typical 10–100 ms per resolve.
- Deterministic timeout behavior via resolver deadline checking in the worker.

### WITHOUT c-ares (fallback)

**Backend**: Standard libc `getaddrinfo()` / `getnameinfo()`, running in a worker thread pool.

**Build Command**:
```bash
cmake --preset dev-linux
cmake --build --preset dev-linux
```

**Test Matrix**: Full 21-test resolver suite passes on Linux with fallback backend.

**Characteristics**:
- Compatible with any POSIX system; no external dependency.
- Higher DNS latency (50–500 ms); suitable for development and compatibility.
- Non-blocking guarantee still holds: blocking calls run in worker threads, not the main event loop.

### Testing Parity

**Key Insight**: Resolver tests are **backend-agnostic**. They don't mock or call c-ares or libc directly. Instead, they:
1. Use public resolver APIs (`addr_resolver_enqueue_*`, `addr_resolver_dequeue_result`).
2. Inject test hooks that work for both backends (`ScopedResolverLookupHook`).
3. Verify contract-level behavior (timeout, dedup, admission, cleanup) that must be identical regardless of backend.

**Parity Verification**: Run the same 21-test suite on both builds:
- No-c-ares Linux: 21/21 ✅
- c-ares Windows: 21/21 ✅ (as of 2026-03-24)

---

## Common Test Patterns and Helpers

### Waiting for Results

**`WaitForResolverResult(resolver_result_t *out, int timeout_ms = 5000)`**

Polls the resolver result queue until a result arrives or timeout:
```cpp
resolver_result_t result = {};
ASSERT_TRUE(WaitForResolverResult(&result));
EXPECT_EQ(result.type, RESOLVER_REQ_REVERSE_CACHE);
EXPECT_TRUE(result.success || result.timed_out);
```

**`WaitForDNSCompletion(int socket_id, int timeout_ms = 5000)`**

Shared helper in `fixtures.hpp` that polls DNS operation state on a socket until completion or timeout (used by socket-layer DNS tests in both extensions and resolver suites).

---

## Lessons Learned

### 1. **Behavior Lockdown Is Essential**

Socket efun behavior must be validated in isolation from DNS, runtime features, and operational enhancements. The `SOCK_BHV_*` tests form a compatibility baseline that protects against accidental API breakage during refactoring.

**Best Practice**: Keep behavior tests minimal and focused on API compliance. Never mix behavior assertions with telemetry checks or performance expectations.

### 2. **Test File Separation Reduces Coupling**

Splitting tests into behavior, extensions, and resolver-contract files:
- Isolates build failures (e.g., dependency or #define issues).
- Clarifies test intent and ownership (behavior vs. operational vs. contract).
- Enables targeted test runs during development (run only resolver tests while debugging resolver changes).
- Reduces build time by allowing selective compilation.

**Best Practice**: Group tests by **what they validate** (API contract vs. feature vs. backend parity), not by **what they test** (socket type, direction, etc.).

### 3. **Explicit Init/Deinit Is Critical for Isolated Tests**

Tests that run in isolation (not through the full driver startup) need explicit initialization of subsystems they depend on. The resolver test harness learned this hard way: tests failed spuriously until `ScopedAsyncRuntime` was added to explicitly call `addr_resolver_init()`.

**Best Practice**:
- Use RAII scoped guards (`ScopedAsyncRuntime`, `ScopedResolverLookupHook`) for every subsystem dependency.
- Don't assume main-loop initialization sequences apply to isolated tests.
- Add assertions (e.g., `IsReady()`) to catch initialization failures early.

### 4. **Backend-Agnostic Tests Ensure Parity**

By testing only public APIs and using test hooks that work across backends, the resolver test suite achieves:
- **Single source of truth**: one test suite validates both c-ares and fallback behavior.
- **Deterministic coverage**: no need for separate "c-ares variant" test files.
- **Confidence in parity**: if all tests pass on both builds, behavior is identical.

**Best Practice**: Test contracts, not implementations. Use test hooks to inject behavior (delay, override) rather than mocking backend libraries.

### 5. **Test Hooks Must Match Worker Semantics**

When forcing timeout behavior via test hooks (e.g., `delay_ms_out`), ensure the delay is injected **in the worker thread**, not in the test thread. The resolver lookup hook is called inside `cares_worker_main()`, so the deadline check in the worker sees the injected latency correctly.

**Best Practice**: Document **where** test hooks execute (main thread vs. worker thread). Understand how the hook affects timing relative to deadline checks in the worker.

---

## Running the Tests

### Run All Socket Efun Tests

```bash
ctest --preset ut-vs16-x64 -R "SocketEfuns"
```

### Run Only Resolver Tests

```bash
ctest --preset ut-vs16-x64 -R "RESOLVER_"
```

### Run a Specific Test Family

```bash
ctest --preset ut-vs16-x64 -R "RESOLVER_FWD_"   # Forward tests only
ctest --preset ut-vs16-x64 -R "RESOLVER_REV_"   # Reverse tests only
ctest --preset ut-vs16-x64 -R "RESOLVER_REFRESH_"  # Refresh tests only
```

### Run Without c-ares (Fallback Backend)

```bash
cmake --preset dev-linux
cmake --build --preset dev-linux
ctest --preset ut-linux -R "SocketEfuns"
```

---

## References

- [Shared Resolver Module Internals](../../docs/internals/addr-resolver.md) — Authoritative implementation and behavior reference for resolver architecture.
- [Async Library Internals](../../docs/internals/async-library.md) — Async runtime and worker integration architecture.
- [src/addr_resolver.cpp](../../src/addr_resolver.cpp) — Shared resolver implementation (c-ares and fallback backends).
- [src/addr_resolver.h](../../src/addr_resolver.h) — Shared resolver public API and test hook declarations.
- [src/comm.c](../../src/comm.c) — Socket layer and resolver integration (query_ip_name, query_addr_name).
- [fixtures.hpp](./fixtures.hpp) — Shared test infrastructure (ScopedAsyncRuntime, ScopedResolverLookupHook, helpers).
