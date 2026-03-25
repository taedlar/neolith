# Async Library User Guide

**Library**: `lib/async`  
**Status**: Production Ready (console worker + built-in DNS resolver integration complete)  
**Platform Support**: Windows, Linux, macOS  
**Implementation**: See [lib/async/](../../lib/async/)

## Overview

The async library infrastructure powers responsive console input and non-blocking DNS resolution in the driver. Mudlib developers don't interact directly with the async library—these features are built-in and automatic.

**Key improvements to driver behavior**:
- Console commands and test bot input are processed immediately
- DNS resolution for socket hostnames doesn't freeze the driver
- Multiple concurrent DNS lookups are handled efficiently with caching

## Implementation Details

The async library uses worker threads to handle blocking I/O operations (console input, DNS resolution) and posts completions back to the main driver event loop. This keeps the LPC interpreter responsive and the heartbeat regular.

**For driver developers** implementing new async-driven features, see [async-library.md](../internals/async-library.md) for the complete component API and design constraints.

## Known Behavior & Limitations

### Console Input
- Test bot and interactive console now receive input immediately
- Windows console supports native line editing
- Both Unix pipes and Windows console are detected automatically

### DNS Resolution
- Hostnames in `socket_connect("hostname port")` are resolved asynchronously
- Multiple concurrent lookups are supported
- Forward and reverse DNS caches reduce repeated lookups
- Configuration: See `ResolverForwardCacheTtl`, `ResolverReverseCacheTtl` in [neolith.conf](../../src/neolith.conf)

### Performance
- Async operations do not consume CPU when idle
- No impact on LPC execution speed or responsiveness during normal heartbeats
- See [dns-resolver.md](dns-resolver.md) for resolver telemetry and diagnostics

## See Also

### Documentation
- **Operator setup**: [dns-resolver.md](dns-resolver.md) - DNS resolver configuration and telemetry
- **Driver internals**: [async-library.md](../internals/async-library.md) - For developers implementing new async features
- **Configuration**: [neolith.conf](../../src/neolith.conf) - Resolver cache TTLs and worker settings

---

**Status**: Production-ready with built-in DNS resolver integration.  
**Questions?** See [CONTRIBUTING.md](../CONTRIBUTING.md)
