# Async DNS Integration Plan

**Status**: Planned  
**Priority**: High  
**Dependencies**: Async library (async_queue, async_worker, async_runtime)

## Problem

Socket efuns currently use **synchronous DNS resolution** (`gethostbyname()`, `gethostbyaddr()`), blocking the entire driver backend during DNS lookups. DNS timeouts (5+ seconds) freeze all game activity.

## Solution

Use async worker threads for DNS resolution:

1. **New socket state**: `DNS_RESOLVING`
2. **Worker pool**: 2-4 threads running `getaddrinfo()` in background
3. **Completion notification**: Workers post results via `async_runtime_post_completion()`
4. **Callback in main thread**: Connection proceeds once DNS resolves

## Modified Socket State Machine

```
UNBOUND → (connect with hostname) → DNS_RESOLVING → BOUND → DATA_XFER
                                          ↓ (error)
                                        CLOSED
```

## Implementation Steps

**Phase 1**: Async library foundation (async_queue, async_worker, async_runtime)

**Phase 2**: DNS integration
- Create `lib/socket/async_dns.{h,c}`
- Add DNS worker pool initialization
- Modify `socket_connect()` to detect hostnames vs IP addresses
- Add DNS completion handler in backend event loop

**Phase 3**: Testing
- DNS timeout handling
- Multiple concurrent lookups
- Error cases (NXDOMAIN, network failure)
- Performance benchmarks

**Estimated Duration**: 2-3 weeks total

## Benefits

- ✅ Eliminates driver blocking on DNS lookups
- ✅ Maintains single-threaded LPC execution semantics
- ✅ Enables concurrent DNS resolution
- ✅ Foundation for other async I/O patterns

## References

- [Async Library Design](../internals/async-library.md) - Core implementation
- [Socket Efuns Implementation](../../lib/socket/socket_efuns.c) - Integration point
