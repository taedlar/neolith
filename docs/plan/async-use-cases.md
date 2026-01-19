# Async Library Use Cases

**Status**: Validated  
**Purpose**: Confirm async library design supports extended use cases beyond console worker

## Overview

The async library (async_queue, async_worker, async_runtime) provides low-level primitives for asynchronous operations. This document validates the API supports diverse use cases without requiring modifications.

## Validated Use Cases

### 1. GUI Console Extension
**Requirement**: Bidirectional communication between driver and GUI clients  
**Solution**: Paired async_queues (from_gui, to_gui) + worker threads for I/O  
**Status**: ✅ Supported with current API

### 2. REST API Calls
**Requirement**: Non-blocking HTTP requests to external services  
**Solution**: Workers execute HTTP calls, correlation IDs match responses to requests  
**Status**: ✅ Supported with current API

### 3. Git Operations
**Requirement**: Long-running git commands without blocking driver  
**Solution**: Workers run git commands, post progress messages to queue  
**Status**: ✅ Supported with current API

### 4. MCP Server
**Requirement**: JSON-RPC over stdio for AI tool integration  
**Solution**: Reader/writer workers for stdio, request/response correlation  
**Status**: ✅ Supported with current API

## Common Patterns

### Bidirectional Channel
GUI clients and MCP server use paired queues:
- Input queue: External → Driver
- Output queue: Driver → External
- Worker threads handle blocking I/O

### Request/Response Correlation
REST API and MCP server add correlation IDs to messages to match responses with requests.

### Progress Reporting
Git operations and long-running tasks post progress messages with percent complete and status text.

## Optional Future Enhancements

These are **not required** but could simplify common patterns:

1. **async_rpc.h** - Request/response correlation helper
2. **async_channel.h** - Bidirectional communication wrapper
3. **async_task.h** - Cancelable tasks with standardized progress

## External Application Libraries

These would **use** the async library but are separate concerns:

1. **lib/http_client/** - HTTP/HTTPS client for REST API calls
2. **lib/git_integration/** - Git command wrapper with progress callbacks
3. **lib/mcp_server/** - Model Context Protocol server implementation

## Conclusion

**The current async library API design is validated.** All proposed use cases work with existing primitives (queue, worker, runtime) without modifications.

**Recommendation**: Proceed with implementation, document patterns in user guide, defer higher-level helpers until real-world demand emerges.

## References

- [Async Library Design](../internals/async-library.md) - Complete technical specification
- [Async User Guide](../manual/async.md) - Usage patterns and examples
