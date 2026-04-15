# Driver Error Routing Internals

This document describes Neolith's current runtime error-routing design after
the migration from jump-based transport to exception-based transport.

## Scope

- Internal transport for runtime errors in driver code.
- Boundary behavior between LPC-visible semantics and C/C++ implementation.
- Master apply routing for uncaught and fatal paths.

This document focuses on current behavior and contracts. It is not a migration
plan.

## Current Design Summary

Neolith uses typed C++ exceptions to transport runtime errors through internal
driver boundaries. Legacy production `setjmp`/`longjmp` transport paths have
been retired.

Primary runtime exception classes:

- `catchable_runtime_error`: normal runtime errors that can be trapped by LPC
  `catch()` boundaries.
- `noncatchable_runtime_limit`: non-catchable resource limits (for example,
  eval-cost exhaustion and deep recursion).
- `fatal_runtime_error`: fatal driver-level failures routed to terminal
  shutdown behavior.

The key implementation is centered in `src/error_context.cpp` plus boundary
wrappers in driver subsystems.

## Core Routing Flow

### 1. Error generation

- `error()` formats and forwards to `error_handler()`.
- `throw()` (efun) stores payload in `catch_value` and then calls
  `throw_error()`.
- `throw(0)` is normalized in the efun implementation to `"*Unspecified error"`.

### 2. Catch-boundary routing

When an active LPC `catch()` boundary exists:

- Catchable errors are routed as `catchable_runtime_error`.
- `catch_value` ownership is managed via scoped guard logic.
- Non-catchable limits are routed as `noncatchable_runtime_limit` and bypass
  catchable handling.

### 3. Uncaught routing

When no active catch boundary handles the error:

- Driver constructs error mapping and invokes `master::error_handler(error)`.
- If `LOG_CATCHES` is enabled, caught-path callback can invoke
  `master::error_handler(error, 1)`.
- If master handler does not provide override output, driver falls back to
  default debug/trace output.

### 4. Fatal routing

- Fatal shutdown path invokes `master::crash(msg, command_giver,
  current_object)` through guarded logic.
- If `master::crash()` itself errors, driver logs and continues shutdown.
- Process terminates via fatal/shutdown policy after crash-hook attempt.

## LPC-Visible Contracts Preserved

- `catch()` success returns `0`.
- Runtime errors exposed via `catch()` are typically `*`-prefixed strings.
- `throw(0)` never returns `0` from `catch()`; it returns normalized
  `"*Unspecified error"`.
- `throw()` without active `catch()` raises `"*Throw with no catch."`.

## Boundary and Safety Notes

- Exception transport is internal to migrated C++ paths; boundary wrappers keep
  ABI behavior stable for C-facing entry points.
- Reentry guard logic protects error reporting from recursive failure loops.
- Heart-beat safety behavior on runtime error remains preserved.

## Toolchain Note

Windows/MSVC and clang-cl validation required explicit `/EHs`-aligned behavior
for consistent exception handling across mixed C/C++ boundaries.

## Related Docs

- [Manual Internals Overview](../manual/internals.md)
- [master::error_handler](../applies/master/error_handler.md)
- [master::crash](../applies/master/crash.md)
- [catch()](../efuns/catch.md)
- [throw()](../efuns/throw.md)
- [error()](../efuns/error.md)
