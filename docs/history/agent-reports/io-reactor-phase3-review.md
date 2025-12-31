# I/O Reactor Phase 3 Design Review: Backend Integration

**Date**: 2025-12-31  
**Status**: In Progress (Console Support Complete)  
**Purpose**: Feasibility analysis for migrating comm.c event loop to reactor API

## Executive Summary

✅ **RECOMMENDATION: Full integration is feasible and recommended**

Analysis confirms that `external_port[]`, interactive users, console, and LPC sockets can all be migrated to the reactor API with minimal structural changes. Console support for Windows is already implemented and tested.

## Current Architecture Problems

### Inefficiency: Redundant Registration

`make_selectmasks()` is called **every backend loop iteration**, rebuilding `poll_fds[]` from scratch for all I/O sources:
- 5 listening sockets (`external_port[]`)
- Up to 250 interactive users (`all_users[]`)
- Console (if enabled)
- LPC sockets
- Address server pipe

This O(N) rebuild happens even when the set of active handles hasn't changed.

### Platform Pollution

Structures leak platform-specific fields:
- `port_def_t` has `poll_index` (only on `HAVE_POLL` platforms)
- `interactive_t` has `poll_index` (only on `HAVE_POLL` platforms)
- Event checking uses conditional `#ifdef` macros throughout business logic

### Complexity

Event processing uses index-based macros with platform conditionals:
```c
#ifdef HAVE_POLL
#define NEW_USER_CAN_READ(i) \
    (external_port[i].poll_index >= 0 && ...)
#else
#define NEW_USER_CAN_READ(i) FD_ISSET(...)
#endif
```

## Reactor API Integration Analysis

### Structural Compatibility

The io_reactor API is **explicitly designed** for this use case:

From [lib/port/io_reactor.h](../../../lib/port/io_reactor.h):
```c
/**
 * @param context User-supplied context pointer. Stored by the reactor and
 *                returned in io_event_t when events occur. Typically points
 *                to interactive_t, port_def_t, or lpc_socket_t. May be NULL.
 */
int io_reactor_add(io_reactor_t *reactor, socket_fd_t fd, 
                   void *context, int events);
```

**Note**: Documentation explicitly mentions `port_def_t` as expected context type.

### Event Delivery Model

**Current approach** (poll-based):
1. `make_selectmasks()` registers all fds → builds `poll_fds[]`
2. `poll()` waits → updates `revents` in `poll_fds[]`
3. `process_io()` checks `poll_fds[external_port[i].poll_index].revents`
4. Calls `new_user_handler(i)` with port **index**

**Reactor approach**:
1. `io_reactor_add()` called once per port during `init_user_conn()`
2. `io_reactor_wait()` returns `io_event_t[]` with ready handles
3. `process_io()` iterates returned events
4. Extracts `port_def_t*` from `event->context`

### Migration Path

#### Option A: Preserve Port Index (Conservative)

Keep `new_user_handler(int which)` signature by computing index from pointer:

```c
void init_user_conn(void) {
    /* ... existing socket setup ... */
    
    g_io_reactor = io_reactor_create();
    
    for (int i = 0; i < 5; i++) {
        if (!external_port[i].port) continue;
        
        // Register listening socket with reactor
        if (io_reactor_add(g_io_reactor, external_port[i].fd,
           Solution Benefits

1. **One-time registration**: Handles registered at creation, not every loop iteration
2. **Platform abstraction**: No `#ifdef` in business logic, reactor handles platform differences
3. **Event-driven processing**: O(events) dispatch instead of O(all_handles) scanning
4. **Unified error handling**: Consistent `EVENT_ERROR` and `EVENT_CLOSE` handling across platforms
5. **Extensibility**: Easy to add new I/O sources without touching event loop core
```

**Pros**:
- Minimal changes to `new_user_handler()`
- Simple pointer arithmetic
- No ABI changes

**Cons**:
- Relies on pointer range checking
- Still couples handler to array index

#### Option B: Refactor to Port Pointer (Clean)

Change `new_user_handler()` to accept `port_def_t*`:

```c
static void new_user_handler(port_def_t *port) {
    socket_fd_t new_socket_fd;
    struct sockaddr_in addr;
    socklen_t length;
    
    if (!port || !port->port) {
        debug_message("new_user_handler: invalid port\n");
        return;
    }
    
   Integration Design Decisions

### Context Identification Strategy

**Decision**: Use pointer range checking (Option A variant)

Event dispatch identifies context type by comparing pointer against known structure arrays:

```c
static inline int is_listening_port(void *context) {
    return (context >= (void*)&external_port[0] &&
            context <  (void*)&external_port[5]);
}
```

**Rationale**:
- Minimal memory overhead (no wrapper structures)
- Leverages existing fixed-size arrays
- Simple implementation
- Used successfully in Phase 2 listening socket tests

### Handler Signature Strategy

**Decision**: Refactor to pointer-based signatures (Option B)

Change handlers from index-based to pointer-based:
- `new_user_handler(int which)` → `new_user_handler(port_def_t *port)`
- Similar changes for user/socket handlers

**Rationale**:
- Cleaner abstraction (no index arithmetic)
- Extensible to dynamic allocation
- Sets precedent for other handlers
- Minimal impact (8 call sites for new_user_handler)

### Console Support

**Status**: ✅ **COMPLETE** for Windows (see [Phase 3 Console Report](io-reactor-phase3-console-support.md))

- Windows: `io_reactor_add_console()` polls `GetNumberOfConsoleInputEvents()` before IOCP
- POSIX: Standard `io_reactor_add(reactor, STDIN_FILENO, ...)`
- Context marker pattern (`CONSOLE_CONTEXT_MARKER`) identifies console events
    // Linear scan of port array
    for (i = 0; i < 5; i++) {
        if (!external_port[i].port) continue;
        if (NEW_USER_CAN_READ(i)) {
            new_user_handler(i);
        }
    }
    /* ... process other fd types ... */
}
```

**After** (reactor-based):
```c
void process_io(io_event_t *events, int num_events) {
    // Event-driven dispatch
    for (i = 0; i < num_events; i++) {
        io_event_t *evt = &events[i];
        
        if (is_listening_port(evt->context)) {
            port_def_t *port = (port_def_t*)evt->context;
            
            if (evt->event_type & EVENT_READ) {
                new_user_handler(port);  // or compute index if needed
            }
            if (evt->event_type & EVENT_ERROR) {
                debug_message("Error on listening port %d\n", port->port);
            }
        }
        else if (is_interactive_user(evt->context)) {
            /* ... handle user events ... */
        }
        /* ... other event types ... */
    }
}
```

**Scalability benefit**: O(events) instead of O(all_registered_fds).

## Error Handling Improvements

The current code doesn't handle errors on listening sockets. The reactor enables this:

```c
void process_io(io_event_t *events, int num_events) {
    for (int i = 0; i < num_events; i++) {
        io_event_t *evt = &events[i];
        port_def_t *port = (port_def_t*)evt->context;
        
        if (evt->event_type & EVENT_ERROR) {
            int error;
            socklen_t len = sizeof(error);
            getsockopt(port->fd, SOL_SOCKET, SO_ERROR, &error, &len);
            
            debug_message("Error on listening port %d (fd=%d): %s\n",
                         port->port, port->fd, strerror(error));
            
            // Attempt recovery or shutdown
            io_reactor_remove(g_io_reactor, port->fd);
            close(port->fd);
            port->port = 0;  // Mark as disabled
        }
        
        if (evt->event_type & EVENT_CLOSE) {
            debug_message("Listening port %d unexpectedly closed\n", port->port);
            /* Recovery logic */
        }
    }
}
```

## Platform Considerations

### POSIX (poll/epoll)

No special handling needed. Listening sockets are just regular fds:

```c
// Linux io_reactor_poll.c or io_reactor_epoll.c
int io_reactor_add(io_reactor_t *reactor, socket_fd_t fd,
                   void *context, int events) {
    // Store context pointer in fd_info table
    reactor->fd_info[fd].context = context;
    reactor->fd_info[fd].events = events;
    
    // Add to poll array or epoll instance
    /* ... platform-specific registration ... */
}
```

### Windows (IOCP)

Listening sockets can use IOCP via `AcceptEx()` for better performance, but standard `accept()` also works:

**Option 1**: Keep `accept()` synchronous (simple migration)
```c
// Windows reactor treats listening sockets as edge-triggered readiness
// Same as POSIX approach
```

**Option 2**: Use `AcceptEx()` for async accepts (future optimization)
```c
// Post async AcceptEx operations
// Requires pre-allocated accept socket and address buffer
// More complex but higher performance for connection storms
```

For Phase 3, recommend **Option 1** (synchronous accept) to minimize changes. `AcceptEx()` can be a Phase 4 enhancement.

## Testing Strategy

### Unit Tests

Add to `tests/test_io_reactor/`:

```c
TEST_F(IoReactorTest, ListeningSocketAccept) {
    // Create listening socket
    socket_fd_t listen_fd = create_listening_socket(9999);
    
    port_def_t test_port = {
        .kind = PORT_TELNET,
        .port = 9999,
        .fd = listen_fd
    };
    
    // Register with reactor
    ASSERT_EQ(0, io_reactor_add(reactor, listen_fd, &test_port, EVENT_READ));
    
    // Connect from client
    socket_fd_t client_fd = connect_to_port(9999);
    
    // Wait for accept readiness
    io_event_t events[10];
    int n = io_reactor_wait(reactor, events, 10, &short_timeout);
    
    ASSERT_EQ(1, n);
    ASSERT_EQ(&test_port, events[0].context);
    ASSERT_TRUE(events[0].event_type & EVENT_READ);
    
    // Accept should succeed
    socket_fd_t accepted_fd = accept(listen_fd, NULL, NULL);
    ASSERT_NE(INVALID_SOCKET_FD, accepted_fd);
}

TEST_F(IoReactorTest, MultipleListeningPorts) {
    port_def_t ports[3];
    
    // Setup 3 listening ports
    for (int iPlatform-Specific Fields

Remove `poll_index` from:
- `port_def_t` (listening sockets)
- `interactive_t` (users)

Reactor internally manages handle→context mapping, eliminating platform leakage.

### 2. Registration Migration

**Before**: `make_selectmasks()` called every loop iteration  
**After**: One-time registration in handle creation functions:
- Listening sockets: `init_user_conn()`
- New users: `new_user_handler()` / `new_interactive()`
- Console: `init_user_conn()` (if console mode enabled)
- LPC sockets: `socket_create()`, `socket_connect()`

**Impact**: Eliminates O(N) rebuild overhead.

### 3. Event Dispatch Refactoring

Replace platform-specific macros (`NEW_USER_CAN_READ`, etc.) with unified event dispatch loop. See [io-reactor.md](../../manual/io-reactor.md) for integration patterns.

## Error Handling Improvements

Reactor enables unified error detection for all I/O sources:
- `EVENT_ERROR`: Network errors, invalid handles
- `EVENT_CLOSE`: Graceful shutdown (FIN received)

Current code relies on `EWOULDBLOCK` during read/write; reactor provides proactive notification.
All handles (listening sockets, user connections, console, pipes) use single `poll()`/`epoll()` backend. No special cases.

### Windows
- **Network sockets**: IOCP for data I/O
- **Listening sockets**: `select()` with zero timeout (hybrid approach from Phase 2)
- **Console**: `GetNumberOfConsoleInputEvents()` polling (Phase 3, complete)

Future enhancement: `AcceptEx()` for async accepts (Phase 4+)- Listening socket integration (multiple ports, context validation)
- Console events (Windows: ✅ 5 tests passing; POSIX: pending)
- User connection events
- Error condition handling

Phase 2 already validated listening socket support with `select()` on Windows.

### Integration Tests
- Full driver startup with reactor
- Multi-port handling (telnet/binary/ascii protocols)
- Stress tests (100+ concurrent connections)
- Memory leak validation (valgrind)Implementation Roadmap

### Phase 3A: Core Integration
- [ ] Refactor `new_user_handler()` signature (port pointer)
- [ ] Add reactor initialization to `init_user_conn()`
- [ ] Add reactor cleanup to `ipc_remove()`
- [ ] Update `do_comm_polling()` to use `io_reactor_wait()`
- [ ] Rewrite `process_io()` for event dispatch
- [ ] Remove `poll_index` from structures
- [ ] Remove `make_selectmasks()` function

### Phase 3B: Console Support
- [x] Windows console implementation
- [x] Windows console tests (5 tests passing)
- [ ] POSIX console testing
- [ ] Console reconnect logic validation

### Phase 3C: Testing & Validation
- [ ] Listening socket integration tests
- [ ] Interactive user migration tests
- [ ] Stress tests (100+ connections)
- [ ] Memory leak validation (valgrind)

### Phase 3D: Documentation
- [ ] Update comm.c implementation comments
- [ ] Document event flow in internals.md
- [ ] Add troubleshooting guide
- [ ] Write Phase 3 completion report**Low Risk** ✅
- Reactor API already designed for this use case (Phase 1/2 validation)
- Minimal structural changes required
- No mudlib-visible API changes
- Platform support proven (19 POSIX tests, 10 Windows tests passing)

**Mitigation**
- Incremental integration (listening sockets → users → console → LPC sockets)
- Debug logging for event dispatch paths
- Stress testing before production deploymentBackend integration is feasible with straightforward migration path:

**Key Benefits**:
1. Eliminates redundant `make_selectmasks()` overhead
2. Removes platform-specific fields from structures
3. Enables unified error handling (EVENT_ERROR, EVENT_CLOSE)
4. Simplifies code (no conditional `#ifdef` macros in event logic)
5. Improves scalability (O(events) vs O(all_handles))

**Design Decisions**:
- Pointer range checking for context identification
- Pointer-based handler signatures
- One-time registration at handle creation
- Console support via platform-specific reactor APIs

**Current Status**:
- ✅ Phase 1/2: Reactor core complete and tested
- ✅ Phase 3: Windows console support complete (5 tests passing)
- ⏳ Phase 3A-D: Backend integration pending

**Next**: Implement Phase 3A core integration (see roadmap above)

---

**Review Status**: ✅ APPROVED FOR IMPLEMENTATION  
**Estimated Effort**: 1-2 weeks