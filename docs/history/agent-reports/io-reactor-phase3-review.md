# I/O Reactor Phase 3 Design Review: external_port Integration

**Date**: 2025-12-31  
**Author**: GitHub Copilot  
**Purpose**: Assess feasibility of integrating `external_port` array with io_reactor API

## Executive Summary

✅ **RECOMMENDATION: Full integration is feasible and recommended**

The `external_port` array can be completely integrated with the io_reactor API. The current design already anticipates this use case, and the migration path is straightforward with minimal structural changes required.

## Current State Analysis

### external_port Structure

Defined in [lib/rc/rc.h](../../../lib/rc/rc.h):

```c
typedef struct {
    int kind;           // PORT_TELNET, PORT_BINARY, or PORT_ASCII
    int port;           // Port number (0 = unused)
    socket_fd_t fd;     // Listening socket file descriptor
#ifdef HAVE_POLL
    int poll_index;     // Index in poll_fds[] array (platform-specific)
#endif
} port_def_t;

extern port_def_t external_port[5];  // Up to 5 listening ports
```

### Current Usage Pattern

The `external_port` array is used in three main phases:

#### 1. Configuration (lib/rc/rc.cpp)
```c
// Parse port specifications from config file
external_port[i].port = strtoul(p, &typ, 0);
external_port[i].kind = PORT_TELNET | PORT_BINARY | PORT_ASCII;
```

#### 2. Socket Creation (src/comm.c:init_user_conn())
```c
for (i = 0; i < 5; i++) {
    if (!external_port[i].port) continue;
    
    // Create listening socket
    external_port[i].fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(..., SO_REUSEADDR, ...);
    bind(..., external_port[i].port, ...);
    set_socket_nonblocking(external_port[i].fd, 1);
    listen(..., SOMAXCONN);
}
```

#### 3. Event Loop (src/comm.c)

**Registration** in `make_selectmasks()`:
```c
for (i = 0; i < 5; i++) {
    if (!external_port[i].port) continue;
    
    poll_fds[i_poll].fd = external_port[i].fd;
    poll_fds[i_poll].events = POLLIN;
    external_port[i].poll_index = i_poll;  // Track index
    i_poll++;
}
```

**Event Processing** in `process_io()`:
```c
for (i = 0; i < 5; i++) {
    if (!external_port[i].port) continue;
    if (NEW_USER_CAN_READ(i)) {
        new_user_handler(i);  // Accept new connection
    }
}
```

Where:
```c
#define NEW_USER_CAN_READ(i) \
    (external_port[i].poll_index >= 0 && \
     (poll_fds[external_port[i].poll_index].revents & POLLIN))
```

### Key Observations

1. **Listening sockets only monitor reads** - new connection events
2. **Context pointer pattern already present** - `poll_index` maps to platform state
3. **Fixed iteration**: Always iterates 0-4, checking `port != 0` for validity
4. **Separate handler**: `new_user_handler(int which)` requires port index

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
                          &external_port[i], EVENT_READ) != 0) {
            debug_fatal("Failed to register port %d with I/O reactor\n",
                       external_port[i].port);
        }
    }
}

void process_io(io_event_t *events, int num_events) {
    for (int i = 0; i < num_events; i++) {
        io_event_t *evt = &events[i];
        
        // Check if event is for a listening port
        if (is_listening_port_event(evt->context)) {
            port_def_t *port = (port_def_t*)evt->context;
            int which = port - external_port;  // Compute index
            
            if (evt->event_type & EVENT_READ) {
                new_user_handler(which);
            }
            if (evt->event_type & EVENT_ERROR) {
                handle_port_error(which);
            }
        }
        /* ... handle other event types ... */
    }
}

// Helper to distinguish port events from user/socket events
static inline int is_listening_port_event(void *context) {
    return (context >= (void*)&external_port[0] &&
            context <  (void*)&external_port[5]);
}
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
    
    length = sizeof(addr);
    new_socket_fd = accept(port->fd, (struct sockaddr*)&addr, &length);
    /* ... existing accept logic ... */
    
    master_ob->interactive->connection_type = port->kind;
#ifdef F_QUERY_IP_PORT
    master_ob->interactive->local_port = port->port;
#endif
    
    ob = mudlib_connect(port->port, inet_ntoa(addr.sin_addr));
    /* ... existing connection setup ... */
    
    if (port->kind == PORT_TELNET) {
        /* Telnet negotiation */
    }
}

void process_io(io_event_t *events, int num_events) {
    for (int i = 0; i < num_events; i++) {
        io_event_t *evt = &events[i];
        
        if (is_listening_port_event(evt->context)) {
            port_def_t *port = (port_def_t*)evt->context;
            
            if (evt->event_type & EVENT_READ) {
                new_user_handler(port);  // Direct pointer
            }
            /* ... error handling ... */
        }
        /* ... other event types ... */
    }
}
```

**Pros**:
- More idiomatic object-oriented design
- Removes array index dependency
- Extensible to dynamic port allocation
- Cleaner abstraction

**Cons**:
- More invasive change to `new_user_handler()`
- All 8 references to the function need signature update

#### Option C: Hybrid Event Dispatch (Flexible)

Use event type tagging for polymorphic dispatch:

```c
typedef enum {
    CONTEXT_LISTENING_PORT,
    CONTEXT_INTERACTIVE_USER,
    CONTEXT_LPC_SOCKET,
    CONTEXT_ADDR_SERVER
} event_context_type_t;

typedef struct {
    event_context_type_t type;
    void *data;
} event_context_t;

// Wrap external_port entries
static event_context_t port_contexts[5];

void init_user_conn(void) {
    /* ... socket setup ... */
    
    for (int i = 0; i < 5; i++) {
        if (!external_port[i].port) continue;
        
        port_contexts[i].type = CONTEXT_LISTENING_PORT;
        port_contexts[i].data = &external_port[i];
        
        io_reactor_add(g_io_reactor, external_port[i].fd,
                      &port_contexts[i], EVENT_READ);
    }
}

void process_io(io_event_t *events, int num_events) {
    for (int i = 0; i < num_events; i++) {
        io_event_t *evt = &events[i];
        event_context_t *ctx = (event_context_t*)evt->context;
        
        switch (ctx->type) {
            case CONTEXT_LISTENING_PORT: {
                port_def_t *port = (port_def_t*)ctx->data;
                if (evt->event_type & EVENT_READ) {
                    new_user_handler(port);
                }
                break;
            }
            case CONTEXT_INTERACTIVE_USER: {
                interactive_t *user = (interactive_t*)ctx->data;
                handle_user_event(user, evt);
                break;
            }
            /* ... other types ... */
        }
    }
}
```

**Pros**:
- Type-safe event dispatch
- Eliminates pointer range checking
- Unified event processing model
- Prepares for future event sources

**Cons**:
- Additional memory overhead (40 bytes for 5 ports)
- More complex initialization
- Indirection through wrapper structure

## Structural Changes Required

### 1. Remove poll_index from port_def_t

**Current**:
```c
typedef struct {
    int kind;
    int port;
    socket_fd_t fd;
#ifdef HAVE_POLL
    int poll_index;  // ← Platform-specific leak
#endif
} port_def_t;
```

**After**:
```c
typedef struct {
    int kind;
    int port;
    socket_fd_t fd;
    // poll_index removed - reactor manages fd→context mapping
} port_def_t;
```

**Impact**: Clean separation of concerns. The reactor internally manages fd tracking.

### 2. Eliminate make_selectmasks() port registration

The `make_selectmasks()` function currently rebuilds `poll_fds[]` **every event loop iteration**. This is inefficient.

**Before**:
```c
// Called EVERY time through backend loop
void make_selectmasks() {
    for (i = 0; i < 5; i++) {
        if (external_port[i].port) {
            poll_fds[i_poll].fd = external_port[i].fd;
            poll_fds[i_poll].events = POLLIN;
            external_port[i].poll_index = i_poll++;
        }
    }
    /* ... register users, sockets, etc. ... */
}
```

**After**:
```c
// Called ONCE during initialization
void init_user_conn() {
    /* ... socket creation ... */
    
    g_io_reactor = io_reactor_create();
    
    for (i = 0; i < 5; i++) {
        if (external_port[i].port) {
            io_reactor_add(g_io_reactor, external_port[i].fd,
                          &external_port[i], EVENT_READ);
        }
    }
}

// make_selectmasks() still exists for interactive users and LPC sockets,
// but no longer processes external_port
```

**Performance benefit**: Eliminates redundant registration overhead.

### 3. Update process_io() event dispatch

**Before** (poll-based):
```c
void process_io() {
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
    for (int i = 0; i < 3; i++) {
        ports[i].port = 10000 + i;
        ports[i].fd = create_listening_socket(ports[i].port);
        io_reactor_add(reactor, ports[i].fd, &ports[i], EVENT_READ);
    }
    
    // Connect to port 10001 (middle port)
    socket_fd_t client = connect_to_port(10001);
    
    // Verify correct port receives event
    io_event_t events[10];
    int n = io_reactor_wait(reactor, events, 10, &short_timeout);
    
    ASSERT_EQ(1, n);
    port_def_t *ready_port = (port_def_t*)events[0].context;
    ASSERT_EQ(10001, ready_port->port);
    ASSERT_EQ(&ports[1], ready_port);
}
```

### Integration Tests

Run full driver with reactor-based external_port:

1. Start mudlib with multiple ports (telnet, binary, ascii)
2. Connect clients to each port
3. Verify correct protocol handling
4. Test rapid connection/disconnection cycles
5. Test connection rejection when max_users reached

## Recommended Implementation Plan

### Phase 3A: Core Integration (Week 1)

1. **Refactor new_user_handler()** to accept `port_def_t*` (Option B)
   - Update all 8 call sites
   - Update signature in header

2. **Move registration to init_user_conn()**
   - Add `io_reactor_add()` calls for each port
   - Remove port registration from `make_selectmasks()`

3. **Update process_io() dispatcher**
   - Add context type checking
   - Route listening socket events to `new_user_handler()`

4. **Remove poll_index from port_def_t**
   - Update struct definition
   - Remove conditional compilation

5. **Add error handling**
   - Handle EVENT_ERROR for listening sockets
   - Log and attempt recovery

### Phase 3B: Testing (Week 1)

6. **Unit tests**
   - Listening socket registration
   - Multiple port handling
   - Context pointer validation

7. **Integration tests**
   - Full driver startup with reactor
   - Multi-port connection handling
   - Protocol negotiation (telnet/binary/ascii)

### Phase 3C: Documentation (Week 1)

8. **Update architecture docs**
   - Document reactor-based port management
   - Update linux-io.md with actual migration
   - Add troubleshooting guide

## Risk Assessment

### Low Risk ✅

- **API compatibility**: Reactor already designed for this use case
- **Structural fit**: Minimal changes to port_def_t
- **Platform support**: Works on both POSIX and Windows
- **Backwards compatibility**: No mudlib-visible changes

### Medium Risk ⚠️

- **Event dispatch complexity**: Need clear context type discrimination
- **Error handling**: New error paths need thorough testing
- **Performance**: Should improve, but needs benchmarking

### Mitigation Strategies

1. **Incremental rollout**: Keep poll-based code paths as fallback during transition
2. **Extensive testing**: Unit tests + integration tests + stress tests
3. **Feature flag**: Add `--use-io-reactor` flag to enable reactor (Phase 3)
4. **Monitoring**: Add debug logging for event dispatch paths

## Conclusion

**The external_port array is an ideal candidate for io_reactor integration.**

Key advantages:
1. ✅ Clean fit with existing reactor API design
2. ✅ Eliminates platform-specific pollution (`poll_index`)
3. ✅ Enables unified error handling
4. ✅ Improves performance (no redundant registration)
5. ✅ Prepares for future enhancements (AcceptEx, etc.)

**Recommended approach**: **Option B** (Refactor to Port Pointer)
- Clean abstraction
- Minimal overhead
- Sets precedent for interactive_t and lpc_socket_t integration

**Estimated effort**: 1-2 weeks including testing and documentation

**Blockers**: None identified

**Next steps**:
1. Implement Option B refactoring
2. Add unit tests for listening socket events
3. Update process_io() event dispatch
4. Document migration in phase3 report

---

**Review Status**: ✅ APPROVED FOR IMPLEMENTATION  
**Phase**: 3A - Core Integration  
**Target**: Neolith v1.0

