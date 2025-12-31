# I/O Reactor Design for Neolith Backend Loop

## Overview

This document provides comprehensive documentation for the I/O reactor design for cross-platform non-blocking I/O in Neolith. The I/O reactor is a platform-agnostic abstraction layer that provides a unified interface for non-blocking I/O multiplexing across different operating systems. It implements the **Reactor Pattern**, decoupling the event detection mechanism from event handling logic.

### Related Documents

For platform-specific implementation details, see:

1. **[Linux I/O Implementation](linux-io.md)**
   - Current `poll()` implementation
   - Migration from existing code
   - Console input handling on Linux
   - Future `epoll()` enhancement
   - Performance characteristics

2. **[Windows I/O Implementation](windows-io.md)**
   - Windows Winsock limitations
   - I/O Completion Ports (IOCP) solution
   - IOCP context structures and lifecycle
   - Console input handling on Windows
   - Performance characteristics

### Current Implementation Status

✅ **Phase 1: Core Abstraction** ([Report](../history/agent-reports/io-reactor-phase1.md))
- [x] Platform-agnostic API defined in [lib/port/io_reactor.h](../../lib/port/io_reactor.h)
- [x] POSIX `poll()` implementation in [lib/port/io_reactor_poll.c](../../lib/port/io_reactor_poll.c)
- [x] Comprehensive unit tests (19 test cases, all passing)
- [x] Build system integration

✅ **Phase 2: Windows IOCP** ([Report](../history/agent-reports/io-reactor-phase2.md))
- [x] Windows IOCP implementation in [lib/port/io_reactor_win32.c](../../lib/port/io_reactor_win32.c)
- [x] IOCP-specific unit tests (4 test cases)
- [x] Listening socket support (6 test cases)
- [x] Hybrid approach: select() for listening sockets, IOCP for data I/O
- [x] Cross-platform test suite (29 total tests, all passing)
- [x] Build system integration for Windows

🔄 **Phase 3: Backend Integration** ([Design Review](../history/agent-reports/io-reactor-phase3-review.md))
- [ ] Integrate `external_port[]` listening sockets
- [ ] Migrate interactive user I/O (`all_users[]`)
- [x] Console mode support (Windows IOCP) ([Report](../history/agent-reports/io-reactor-phase3-console-support.md))
- [ ] Console mode support (POSIX)
- [ ] LPC socket efuns integration
- [ ] Replace `make_selectmasks()`/`process_io()` with reactor event loop

⬜ **Phase 4: Future Enhancements**
- [ ] Linux `epoll()` backend
- [ ] BSD/macOS `kqueue()` support
- [ ] Performance benchmarking

## Reactor Pattern Fundamentals

The reactor pattern is a design pattern for handling service requests delivered concurrently to an application by demultiplexing events and dispatching them to appropriate handlers.

### Key Components

1. **Handles**: File descriptors, sockets, or other I/O sources
2. **Event Demultiplexer**: Platform-specific mechanism (poll, epoll, IOCP, kqueue)
3. **Event Handlers**: Callbacks that process specific event types
4. **Reactor**: Orchestrates event detection and handler dispatch

### Flow

```mermaid
graph TB
    subgraph "Event Loop (backend)"
        direction TB
        Register["1. Register Handles<br/>(io_reactor_add)"]
        Detect{"Listening<br/>Socket?"}
        TrackListen["Track in<br/>listen_sockets[]"]
        PostIOCP["Post WSARecv<br/>to IOCP"]
        Wait["2. Wait for Events<br/>(io_reactor_wait)"]
        CheckListen["select() on<br/>listening sockets"]
        CheckIOCP["GetQueuedCompletionStatus<br/>(IOCP)"]
        Return["4. Return Events<br/>(io_event_t array)"]
        Process["5. Process Events<br/>(process_io)"]
        
        Register --> Detect
        Detect -->|Yes<br/>Windows| TrackListen
        Detect -->|No| PostIOCP
        TrackListen --> Wait
        PostIOCP --> Wait
        Wait --> CheckListen
        CheckListen --> CheckIOCP
        CheckIOCP --> Return
        Return --> Process
        Process -.->|"Loop continues"| Wait
    end
    
    style Detect fill:#ff9,stroke:#333,stroke-width:2px
    style TrackListen fill:#9cf,stroke:#333,stroke-width:2px
    style CheckListen fill:#9cf,stroke:#333,stroke-width:2px
```

**Note**: On Windows, listening sockets use `select()` (readiness-based) while connected sockets use IOCP (completion-based). POSIX platforms use a single demultiplexer (`poll()` or `epoll()`) for all socket types.

**Event Loop:**
1. Application registers handles with the reactor
2. Reactor waits for events on registered handles (blocking with timeout)
3. Event demultiplexer returns when events occur
4. Reactor dispatches events to registered handlers
5. Handlers process events
6. Loop repeats

## Design Goals

1. **Platform Portability**: Single codebase for Linux, Windows, BSD, etc.
2. **Code Clarity**: Minimize `#ifdef` pollution in business logic
3. **Scalability**: Efficient handling of hundreds/thousands of connections
4. **Performance**: Low-latency I/O for interactive MUD experience
5. **Maintainability**: Clear separation between platform and application concerns
6. **Console Support**: Unified handling of network sockets and console I/O

## I/O Reactor Abstraction API

The reactor API is defined in [lib/port/io_reactor.h](../../lib/port/io_reactor.h). Key components:

### Event Types
- `EVENT_READ` - Socket/fd is readable
- `EVENT_WRITE` - Socket/fd is writable  
- `EVENT_ERROR` - Error occurred
- `EVENT_CLOSE` - Connection closed

### Core Structures
- `io_event_t` - Event returned by reactor (context, type, optional buffer)
- `io_reactor_t` - Opaque reactor handle

### Functions
- **Lifecycle**: `io_reactor_create()`, `io_reactor_destroy()`
- **Registration**: `io_reactor_add()`, `io_reactor_modify()`, `io_reactor_remove()`
- **Event Loop**: `io_reactor_wait()` - core demultiplexing function
- **Platform Helpers**: `io_reactor_post_read()`, `io_reactor_post_write()` (no-ops on POSIX)

See the header file for complete API documentation with detailed parameter descriptions.

## Phase 3: Backend Integration Design

This section details the migration of [src/comm.c](../../src/comm.c) from direct `poll()`/`select()` to the io_reactor API. For complete design rationale, see [Phase 3 Design Review](../history/agent-reports/io-reactor-phase3-review.md).

### Migration Scope

The current event loop in [comm.c](../../src/comm.c) manages four types of I/O sources:

1. **Listening Sockets** (`external_port[5]`) - Accept new connections
2. **Interactive Users** (`all_users[]`) - Connected players
3. **Console Input** (`STDIN_FILENO` in console mode) - Local admin terminal
4. **LPC Sockets** (`lpc_socks[]`) - Efun-created sockets
5. **Address Server** (`addr_server_fd`) - Async DNS resolution

All will be migrated to use reactor-based event handling.

### Current Architecture Problems

**Inefficiency**: `make_selectmasks()` is called **every backend loop iteration**, rebuilding the entire `poll_fds[]` array from scratch:

```c
void backend_loop() {
    while (!game_is_being_shut_down) {
        make_selectmasks();  // ← Rebuilds entire fd array every ~2 seconds!
        poll(poll_fds, ...);
        process_io();
    }
}
```

**Platform Pollution**: Structures leak platform-specific fields:

```c
typedef struct {
    int kind;
    int port;
    socket_fd_t fd;
#ifdef HAVE_POLL
    int poll_index;  // ← Platform leakage!
#endif
} port_def_t;
```

**Complexity**: Event processing uses index-based macros with conditional compilation:

```c
#ifdef HAVE_POLL
#define NEW_USER_CAN_READ(i) \
    (external_port[i].poll_index >= 0 && \
     (poll_fds[external_port[i].poll_index].revents & POLLIN))
#else
#define NEW_USER_CAN_READ(i) FD_ISSET(external_port[i].fd, &readmask)
#endif
```

### Reactor Solution Benefits

1. **One-time registration**: Sockets registered once when created, not every loop
2. **Platform abstraction**: No `#ifdef` needed in business logic
3. **Event-driven**: O(events) processing instead of O(all_fds)
4. **Unified error handling**: Consistent `EVENT_ERROR` and `EVENT_CLOSE` handling
5. **Extensibility**: Easy to add new I/O sources

---

## Integration with Neolith Backend

### 1. Listening Socket Integration (`external_port[]`)

The `external_port` array tracks up to 5 listening sockets (defined in [lib/rc/rc.h](../../lib/rc/rc.h)):

```c
typedef struct {
    int kind;           // PORT_TELNET, PORT_BINARY, or PORT_ASCII
    int port;           // Port number (0 = unused slot)
    socket_fd_t fd;     // Listening socket file descriptor
} port_def_t;

extern port_def_t external_port[5];
```

#### Current Pattern (Inefficient)

**Socket Creation** (`init_user_conn()`):
```c
for (i = 0; i < 5; i++) {
    if (!external_port[i].port) continue;
    external_port[i].fd = socket(...);
    bind(..., external_port[i].port, ...);
    listen(...);
}
```

**Registration** (`make_selectmasks()` - called every loop!):
```c
for (i = 0; i < 5; i++) {
    if (!external_port[i].port) continue;
    poll_fds[i_poll].fd = external_port[i].fd;
    poll_fds[i_poll].events = POLLIN;
    external_port[i].poll_index = i_poll++;  // Track index
}
```

**Event Processing** (`process_io()`):
```c
for (i = 0; i < 5; i++) {
    if (!external_port[i].port) continue;
    if (NEW_USER_CAN_READ(i)) {
        new_user_handler(i);  // Accept connection
    }
}
```

#### Reactor Pattern (Efficient)

**Socket Creation + Registration** (`init_user_conn()` - one time):
```c
void init_user_conn(void) {
    /* ... existing socket setup ... */
    
    g_io_reactor = io_reactor_create();
    if (!g_io_reactor) {
        debug_fatal("Failed to create I/O reactor\n");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < 5; i++) {
        if (!external_port[i].port) continue;
        
        /* ... socket(), bind(), listen() ... */
        
        // Register listening socket with reactor (NEW)
        if (io_reactor_add(g_io_reactor, external_port[i].fd,
                          &external_port[i], EVENT_READ) != 0) {
            debug_fatal("Failed to register port %d with I/O reactor\n",
                       external_port[i].port);
        }
    }
}
```

**Event Processing** (reactor-driven):
```c
void process_io(io_event_t *events, int num_events) {
    for (int i = 0; i < num_events; i++) {
        io_event_t *evt = &events[i];
        
        // Identify event source by context pointer
        if (is_listening_port(evt->context)) {
            port_def_t *port = (port_def_t*)evt->context;
            
            if (evt->event_type & EVENT_READ) {
                new_user_handler(port);  // Accept using port pointer
            }
            if (evt->event_type & EVENT_ERROR) {
                debug_message("Error on listening port %d\n", port->port);
                // Attempt recovery or disable port
            }
        }
        /* ... handle other event types ... */
    }
}

// Helper to distinguish listening ports from other contexts
static inline int is_listening_port(void *context) {
    return (context >= (void*)&external_port[0] &&
            context <  (void*)&external_port[5]);
}
```

**Handler Signature Update**:
```c
// Old: Takes array index
static void new_user_handler(int which) {
    port_def_t *port = &external_port[which];
    /* ... */
}

// New: Takes port pointer directly
static void new_user_handler(port_def_t *port) {
    if (!port || !port->port) return;
    
    new_socket_fd = accept(port->fd, ...);
    /* ... existing accept logic ... */
    
    master_ob->interactive->connection_type = port->kind;
    master_ob->interactive->local_port = port->port;
    
    ob = mudlib_connect(port->port, inet_ntoa(addr.sin_addr));
    /* ... */
}
```

**Structural Changes**:
- Remove `poll_index` from `port_def_t` (no longer needed)
- Remove port registration from `make_selectmasks()`
- Update all 8 call sites of `new_user_handler()` to pass pointer

---

### 2. Console Mode Integration

Console mode connects stdin to an interactive user object for local administration. Platform differences require special handling:

- **POSIX**: Register `STDIN_FILENO` as a regular fd using `io_reactor_add()`
- **Windows**: Use `io_reactor_add_console()` - consoles cannot be added to IOCP, so reactor polls `GetNumberOfConsoleInputEvents()` before blocking

Console events are identified by a unique context marker (`CONSOLE_CONTEXT_MARKER`) to distinguish from network sockets.

**Implementation**:
- API: [lib/port/io_reactor.h](../../lib/port/io_reactor.h) - `io_reactor_add_console()` (Windows only)
- Windows: [lib/port/io_reactor_win32.c](../../lib/port/io_reactor_win32.c) - Console polling in `io_reactor_wait()`
- Tests: [tests/test_io_reactor/test_io_reactor_console.cpp](../../tests/test_io_reactor/test_io_reactor_console.cpp) - 5 test cases, all passing

**Details**: See [Phase 3 Console Support Report](../history/agent-reports/io-reactor-phase3-console-support.md) for complete implementation details, code examples, and integration patterns.

---

### 3. Interactive User Integration

Current implementation tracks users in `all_users[]` array with platform-specific indices.

#### Reactor Migration

**User Connection** (`new_user_handler()`):
```c
static void new_user_handler(port_def_t *port) {
    new_socket_fd = accept(port->fd, ...);
    
    // Create interactive structure
    new_interactive(new_socket_fd);
    
    // Register with reactor (NEW)
    if (io_reactor_add(g_io_reactor, new_socket_fd,
                      master_ob->interactive, EVENT_READ) != 0) {
        debug_message("Failed to register new user socket\n");
        remove_interactive(master_ob, 0);
        return;
    }
    
    /* ... existing connection setup ... */
}
```

**User Disconnection**:
```c
void remove_interactive(object_t *ob, int dested) {
    if (ob->interactive) {
        // Unregister from reactor (NEW)
        io_reactor_remove(g_io_reactor, ob->interactive->fd);
        
        if (ob->interactive->fd != STDIN_FILENO) {
            SOCKET_CLOSE(ob->interactive->fd);
        }
    }
    
    /* ... existing cleanup ... */
}
```

**Write Notification**:

When output buffer fills, request write notification:

```c
int flush_message(interactive_t *ip) {
    /* ... attempt to send ... */
    
    if (num_bytes < ip->message_length) {
        // Partial write - enable write events
        io_reactor_modify(g_io_reactor, ip->fd, EVENT_READ | EVENT_WRITE);
    } else {
        // Full write - disable write events
        io_reactor_modify(g_io_reactor, ip->fd, EVENT_READ);
    }
}
```

**Remove `poll_index` from `interactive_t`**:

The `interactive_t` structure (in [src/comm.h](../../src/comm.h)) currently has:

```c
typedef struct interactive_s {
    /* ... */
#ifdef HAVE_POLL
    int poll_index;  // ← Remove this
#endif
    /* ... */
} interactive_t;
```

This field becomes unnecessary when using the reactor.

---

### 4. Initialization

In [src/comm.c](../../src/comm.c), replace direct `poll()`/`select()` usage:

```c
#include "port/io_reactor.h"

static io_reactor_t *g_io_reactor = NULL;

void init_user_conn(void) {
    /* ... existing socket setup for external_port[] ... */
    
    // Create I/O reactor
    g_io_reactor = io_reactor_create();
    if (!g_io_reactor) {
        debug_fatal("Failed to create I/O reactor\n");
        exit(EXIT_FAILURE);
    }
    
    // Register listening sockets
    for (int i = 0; i < 5; i++) {
        if (!external_port[i].port) continue;
        
        if (io_reactor_add(g_io_reactor, external_port[i].fd,
                          &external_port[i], EVENT_READ) != 0) {
            debug_fatal("Failed to register port %d with I/O reactor\n",
                       external_port[i].port);
        }
    }
    
    // Register console if in console mode
    if (MAIN_OPTION(console_mode)) {
#ifdef _WIN32
        if (io_reactor_add_console(g_io_reactor, CONSOLE_CONTEXT_MARKER) != 0) {
            debug_message("Warning: Failed to register console input\n");
        }
#else
        if (io_reactor_add(g_io_reactor, STDIN_FILENO,
                          CONSOLE_CONTEXT_MARKER, EVENT_READ) != 0) {
            debug_message("Warning: Failed to register console input\n");
        }
#endif
    }
}

void ipc_remove(void) {
    /* ... existing cleanup ... */
    
    if (g_io_reactor) {
        io_reactor_destroy(g_io_reactor);
        g_io_reactor = NULL;
    }
}
```

### 5. Event Loop

Replace `do_comm_polling()` and `process_io()` to use reactor:

**Main Event Wait** (replaces poll/select):
```c
// Global event buffer (reused across iterations)
static io_event_t g_io_events[MAX_IO_EVENTS];
static int g_num_io_events = 0;

int do_comm_polling(struct timeval *timeout) {
    opt_trace(TT_BACKEND|3, "do_comm_polling: timeout %ld sec, %ld usec",
              timeout->tv_sec, timeout->tv_usec);
    
    g_num_io_events = io_reactor_wait(g_io_reactor, g_io_events,
                                       MAX_IO_EVENTS, timeout);
    return g_num_io_events;
}
```

**Event Processing** (replaces linear fd scanning):
```c
void process_io(void) {
    for (int i = 0; i < g_num_io_events; i++) {
        io_event_t *evt = &g_io_events[i];
        
        // Dispatch based on context type
        if (evt->context == CONSOLE_CONTEXT_MARKER) {
            handle_console_event(evt);
        }
        else if (is_listening_port(evt->context)) {
            handle_listening_port_event(evt);
        }
        else if (is_interactive_user(evt->context)) {
            handle_user_event(evt);
        }
        else if (is_lpc_socket(evt->context)) {
            handle_lpc_socket_event(evt);
        }
        else if (evt->context == (void*)&addr_server_fd) {
            handle_addr_server_event(evt);
        }
        else {
            debug_message("Unknown event context: %p\n", evt->context);
        }
    }
}

// Context type identification helpers
static inline int is_listening_port(void *context) {
    return (context >= (void*)&external_port[0] &&
            context <  (void*)&external_port[5]);
}

static inline int is_interactive_user(void *context) {
    // Check if pointer is in all_users array range
    for (int i = 0; i < max_users; i++) {
        if (all_users[i] == context) return 1;
    }
    return 0;
}

static inline int is_lpc_socket(void *context) {
#ifdef PACKAGE_SOCKETS
    return (context >= (void*)&lpc_socks[0] &&
            context <  (void*)&lpc_socks[max_lpc_socks]);
#else
    return 0;
#endif
}

// Event handlers
static void handle_console_event(io_event_t *evt) {
    if (evt->event_type & EVENT_READ) {
        if (!console_user_connected()) {
            init_console_user(1);  // Reconnect console user
        }
    }
}

static void handle_listening_port_event(io_event_t *evt) {
    port_def_t *port = (port_def_t*)evt->context;
    
    if (evt->event_type & EVENT_READ) {
        new_user_handler(port);  // Accept new connection
    }
    if (evt->event_type & EVENT_ERROR) {
        debug_message("Error on listening port %d (fd=%d)\n",
                     port->port, port->fd);
        // Could attempt recovery or mark port as failed
    }
}

static void handle_user_event(io_event_t *evt) {
    interactive_t *ip = (interactive_t*)evt->context;
    
    // Validate user still exists
    if (!ip || !ip->ob || (ip->ob->flags & O_DESTRUCTED)) {
        return;
    }
    
    if (evt->event_type & EVENT_CLOSE) {
        // Connection closed by remote
        remove_interactive(ip->ob, 0);
        return;
    }
    
    if (evt->event_type & EVENT_ERROR) {
        // Network error occurred
        ip->iflags |= NET_DEAD;
        remove_interactive(ip->ob, 0);
        return;
    }
    
    if (evt->event_type & EVENT_READ) {
        // Data available to read
        get_user_data(ip);
        
        // Check if user was disconnected during processing
        if (!ip || !ip->ob) return;
    }
    
    if (evt->event_type & EVENT_WRITE) {
        // Socket is writable (after partial write)
        flush_message(ip);
        
        // If buffer now empty, disable write notifications
        if (ip->message_length == 0) {
            io_reactor_modify(g_io_reactor, ip->fd, EVENT_READ);
        }
    }
}

static void handle_lpc_socket_event(io_event_t *evt) {
#ifdef PACKAGE_SOCKETS
    lpc_socket_t *sock = (lpc_socket_t*)evt->context;
    
    if (evt->event_type & EVENT_READ) {
        socket_read_select_handler(sock - lpc_socks);  // Compute index
    }
    if (evt->event_type & EVENT_WRITE) {
        socket_write_select_handler(sock - lpc_socks);
    }
    if (evt->event_type & (EVENT_ERROR | EVENT_CLOSE)) {
        // Handle socket error/close
        socket_close(sock - lpc_socks);
    }
#endif
}

static void handle_addr_server_event(io_event_t *evt) {
    if (evt->event_type & EVENT_READ) {
        hname_handler();  // Existing addr server handler
    }
}
```

---

### 6. Remove make_selectmasks()

The `make_selectmasks()` function becomes **obsolete**. All registration happens at handle creation time:

- **Listening sockets**: Registered in `init_user_conn()`
- **New users**: Registered in `new_user_handler()` via `new_interactive()`
- **Console**: Registered in `init_user_conn()` (if enabled)
- **LPC sockets**: Registered in `socket_create()`, `socket_connect()`, etc.
- **Addr server**: Registered when server pipe is created

**Migration steps**:
1. Keep `make_selectmasks()` as empty stub initially
2. Move registration logic to appropriate creation points
3. Remove `make_selectmasks()` entirely once verified
4. Remove `poll_fds[]`, `readmask`, `writemask` globals

---

### 7. Platform-Specific Considerations

#### POSIX (Linux, macOS, BSD)

**poll() Implementation** (already complete in Phase 1):
- All handles (sockets, STDIN, pipes) use same `poll()` array
- No special cases needed
- Console integrated seamlessly

**Future epoll() Enhancement** (Phase 4):
- Drop-in replacement for `io_reactor_poll.c`
- Better scalability for >100 connections
- No application code changes required

#### Windows

**IOCP Implementation** (complete in Phase 2):
- Network sockets use IOCP for data I/O
- Listening sockets use `select()` (hybrid approach)
- Console uses `GetNumberOfConsoleInputEvents()` polling

**Console Handling** (NEW in Phase 3):
- `io_reactor_add_console()` stores console handle in reactor state
- `io_reactor_wait()` checks console before blocking on IOCP
- Console events delivered as `EVENT_READ` with `CONSOLE_CONTEXT_MARKER`

---

### 8. Error Handling Improvements

The reactor enables **unified error detection**:

**Listening Socket Errors**:
```c
if (evt->event_type & EVENT_ERROR) {
    int error;
    socklen_t len = sizeof(error);
    getsockopt(port->fd, SOL_SOCKET, SO_ERROR, &error, &len);
    
    debug_message("Error on listening port %d: %s\n",
                 port->port, strerror(error));
    
    // Attempt recovery
    io_reactor_remove(g_io_reactor, port->fd);
    close(port->fd);
    // Could attempt to recreate socket
}
```

**User Connection Errors**:
```c
if (evt->event_type & EVENT_ERROR) {
    // Network error - immediate disconnect
    ip->iflags |= NET_DEAD;
    remove_interactive(ip->ob, 0);
}

if (evt->event_type & EVENT_CLOSE) {
    // Graceful close (FIN received)
    remove_interactive(ip->ob, 0);
}
```

**Current code doesn't detect these conditions** - relies on EWOULDBLOCK during read/write.

---

### 9. Testing Strategy for Phase 3

#### Unit Tests

Tests use `TEST()` macros (not `TEST_F()` fixtures) since each test is independent. Global `WinsockEnvironment` in [test_io_reactor_main.cpp](../../tests/test_io_reactor/test_io_reactor_main.cpp) handles Windows socket initialization.

Add to `tests/test_io_reactor/`:

```cpp
TEST(IOReactorListenTest, ListeningSocketIntegration) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(nullptr, reactor);
    
    // Simulate external_port registration
    struct port_def_t test_port = {PORT_TELNET, 9999, 0};
    test_port.fd = create_listening_socket(9999);
    
    ASSERT_EQ(0, io_reactor_add(reactor, test_port.fd, &test_port, EVENT_READ));
    
    // Connect client
    socket_fd_t client = connect_to_port(9999);
    
    // Verify listening socket receives event
    io_event_t events[10];
    int n = io_reactor_wait(reactor, events, 10, &short_timeout);
    
    ASSERT_EQ(1, n);
    ASSERT_EQ(&test_port, events[0].context);
    ASSERT_TRUE(events[0].event_type & EVENT_READ);
    
    close_socket_pair(client, test_port.fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorConsoleTest, ConsoleEventPOSIX) {
#ifndef _WIN32
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(nullptr, reactor);
    
    void *console_marker = (void*)0x1;
    
    // Register STDIN
    ASSERT_EQ(0, io_reactor_add(reactor, STDIN_FILENO, console_marker, EVENT_READ));
    
    io_reactor_destroy(reactor);
    // Note: Actual input testing requires test harness or integration tests
#endif
}

TEST(IOReactorConsoleTest, ConsoleEventWindows) {
#ifdef _WIN32
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(nullptr, reactor);
    
    void *console_marker = (void*)0x1;
    
    ASSERT_EQ(0, io_reactor_add_console(reactor, console_marker));
    
    io_reactor_destroy(reactor);
    // Note: Actual console input testing may skip if stdin is redirected
#endif
}
```

#### Integration Tests

**With Example Mudlib**:
1. Start driver with 3 ports (telnet 4000, binary 4001, ascii 4002)
2. Connect 10 clients to each port simultaneously
3. Send large messages (>8KB) to test write buffering
4. Test rapid connect/disconnect cycles
5. Test console mode on both Windows and POSIX

**Stress Testing**:
- 100+ concurrent connections
- Sustained message throughput (MB/sec)
- Connection churn (100 connects/sec)

---

### 10. Migration Checklist

**Phase 3A: Core Integration**
- [x] Design documented (this section)
- [ ] Refactor `new_user_handler()` signature (port pointer)
- [ ] Add reactor initialization to `init_user_conn()`
- [ ] Add reactor cleanup to `ipc_remove()`
- [ ] Update `do_comm_polling()` to use `io_reactor_wait()`
- [ ] Rewrite `process_io()` for event dispatch
- [ ] Remove `poll_index` from `port_def_t` and `interactive_t`
- [ ] Remove `make_selectmasks()` function

**Phase 3B: Console Support**
- [x] Implement `io_reactor_add_console()` for Windows
- [ ] Test console mode on POSIX
- [x] Test console mode on Windows
- [ ] Verify console reconnect logic

**Phase 3C: Testing**
- [ ] Unit tests for listening socket integration
- [x] Unit tests for console events (Windows: 5 tests passing)
- [ ] Integration tests with example mudlib
- [ ] Stress tests (100+ connections)
- [ ] Verify no memory leaks (valgrind)

**Phase 3D: Documentation**
- [ ] Update [comm.c](../../src/comm.c) comments
- [ ] Document reactor event flow in [internals.md](internals.md)
- [ ] Add troubleshooting guide for I/O issues
- [ ] Write Phase 3 completion report

---

## Complete Event Flow Diagram

```mermaid
graph TB
    subgraph "Initialization (init_user_conn)"
        CreateReactor["io_reactor_create()"]
        RegPorts["Register external_port[i]<br/>context = &external_port[i]"]
        RegConsole["Register console (if enabled)<br/>context = CONSOLE_MARKER"]
        CreateReactor --> RegPorts
        RegPorts --> RegConsole
    end
    
    subgraph "New Connection (new_user_handler)"
        Accept["accept() on listening socket"]
        NewInteractive["new_interactive(fd)"]
        RegUser["io_reactor_add(fd, ip, EVENT_READ)"]
        Accept --> NewInteractive
        NewInteractive --> RegUser
    end
    
    subgraph "Event Loop (backend)"
        Wait["io_reactor_wait()<br/>Blocks until events"]
        ProcessEvents["process_io()<br/>Dispatch by context type"]
        
        Wait --> ProcessEvents
        
        ProcessEvents --> CheckConsole{"Console?"}
        CheckConsole -->|Yes| HandleConsole["init_console_user()"]
        
        CheckConsole -->|No| CheckPort{"Listening Port?"}
        CheckPort -->|Yes| HandlePort["new_user_handler(port)"]
        HandlePort --> Accept
        
        CheckPort -->|No| CheckUser{"Interactive User?"}
        CheckUser -->|Yes| HandleUser["get_user_data(ip)<br/>flush_message(ip)"]
        
        CheckUser -->|No| CheckSocket{"LPC Socket?"}
        CheckSocket -->|Yes| HandleSocket["socket_read/write_handler()"]
        
        CheckSocket -->|No| HandleOther["addr_server, etc."]
        
        HandleConsole -.-> Wait
        HandleUser -.-> Wait
        HandleSocket -.-> Wait
        HandleOther -.-> Wait
    end
    
    subgraph "Disconnect"
        Remove["remove_interactive(ob)"]
        Unreg["io_reactor_remove(fd)"]
        Close["close(fd)"]
        Remove --> Unreg
        Unreg --> Close
    end
    
    RegConsole -.-> Wait
    RegUser -.-> Wait
    HandleUser --> Remove
    
    style Wait fill:#9cf,stroke:#333,stroke-width:2px
    style ProcessEvents fill:#ff9,stroke:#333,stroke-width:2px
```

---

## Platform Implementation Requirements

Each platform must implement the reactor interface defined above. See platform-specific documents:

- [Linux I/O Reactor Design](linux-io.md) - Using `poll()` or `epoll()`
- [Windows I/O Reactor Design](windows-io.md) - Using I/O Completion Ports (IOCP)

### Windows Listening Socket Handling

**Challenge**: Windows IOCP is completion-based (notifies when I/O operations complete), but listening sockets don't perform I/O—they only become "ready" to accept connections.

**Solution**: Hybrid approach implemented in [io_reactor_win32.c](../../lib/port/io_reactor_win32.c):

1. **Detection**: Use `getsockopt(SO_ACCEPTCONN)` to identify listening sockets during `io_reactor_add()`
2. **Tracking**: Store listening sockets in a separate `listen_sockets[]` array (dynamically resized)
3. **Polling**: In `io_reactor_wait()`, call `select()` with zero timeout on listening sockets before blocking on IOCP
4. **Event Delivery**: Return `EVENT_READ` when listening socket is ready to accept

**Rationale**: This allows the reactor to provide a unified API while leveraging the optimal mechanism for each socket type:
- Listening sockets → `select()` (readiness notification)
- Connected sockets → IOCP (completion notification with zero-copy data delivery)

See [Windows I/O Implementation](windows-io.md) for complete details.

### Implementation Files

```
lib/port/
├── io_reactor.h           # Platform-agnostic API (this document)
├── io_reactor_poll.c      # POSIX poll() implementation
├── io_reactor_epoll.c     # Linux epoll() implementation (future)
├── io_reactor_kqueue.c    # BSD/macOS kqueue() implementation (future)
└── io_reactor_win32.c     # Windows IOCP implementation
```

### CMake Integration

✅ **Current Implementation** ([lib/port/CMakeLists.txt](../../lib/port/CMakeLists.txt)):

```cmake
# lib/port/CMakeLists.txt
set(port_SOURCES
    # ... other sources ...
    # I/O Reactor - platform-specific implementation selection
    $<$<NOT:$<PLATFORM_ID:Windows>>:io_reactor_poll.c>
)

target_sources(port INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ..
    FILES ... io_reactor.h socket_comm.h
)
```

**Future Platform Selection**:
```cmake
# Phase 2: Add Windows support
if(WIN32)
    target_sources(port PRIVATE io_reactor_win32.c)
else()
    target_sources(port PRIVATE io_reactor_poll.c)
endif()

# Phase 4: Optimize with epoll on Linux
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND HAVE_EPOLL)
    target_sources(port PRIVATE io_reactor_epoll.c)
```

## Testing Strategy

### Unit Tests: `tests/test_io_reactor/`

✅ **Phase 1 & 2 Complete** - Comprehensive GoogleTest suite implemented:

**Test Coverage** (29 test cases, all passing):
- **Lifecycle** (3 tests): CreateDestroy, CreateMultiple, DestroyNull
- **Registration** (6 tests): AddRemoveSocket, AddWithContext, AddDuplicateFails, RemoveNonExistent, ModifyEvents, ModifyNonExistentFails
- **Event Wait** (5 tests): TimeoutNoEvents, EventDelivery, MultipleEvents, MaxEventsLimitation, WriteEvent
- **Error Handling** (2 tests): InvalidParameters, AddInvalidFd
- **Scalability** (1 test): ManyConnections (100 socket pairs)
- **Platform Helpers** (2 tests): PostReadNoOp, PostWriteNoOp
- **Listening Sockets** (6 tests): BasicListenAccept, MultipleListeningPorts, MultipleSimultaneousConnections, ContextPointerRangeCheck, ListenWithUserSockets, NoEventsWhenNoConnections
- **IOCP-Specific** (4 tests): CompletionWithDataInBuffer, GracefulClose, CancelledOperations, MultipleReadsOnSameSocket

**Running Tests**:
```bash
# Run reactor tests only (Linux)
ctest --preset ut-linux --tests-regex IOReactor --output-on-failure

# Run reactor tests only (Windows)
ctest --preset ut-vs16-x64 --tests-regex IOReactor --output-on-failure

# Run all tests
ctest --preset ut-linux   # or ut-vs16-x64 on Windows
```

**Results**: 
- Linux: `100% tests passed, 0 tests failed out of 19` (0.12s runtime)
- Windows: `100% tests passed, 0 tests failed out of 29` (1.69s runtime)

See complete test implementations:
- [tests/test_io_reactor/test_io_reactor_basic.cpp](../../tests/test_io_reactor/test_io_reactor_basic.cpp)
- [tests/test_io_reactor/test_io_reactor_listen.cpp](../../tests/test_io_reactor/test_io_reactor_listen.cpp)
- [tests/test_io_reactor/test_io_reactor_iocp.cpp](../../tests/test_io_reactor/test_io_reactor_iocp.cpp)

### Integration Tests

⬜ **Phase 3: Backend Integration** - Planned stress testing with actual mudlib:

1. **Multi-player Load**: 100+ concurrent connections
2. **Console Mode**: Verify STDIN handling works  
3. **Message Throughput**: Large messages, rapid sends
4. **Connection Churn**: Rapid connect/disconnect cycles

### Platform Coverage

✅ **Current**: Linux/POSIX (poll-based implementation)
- Tested on Linux (Ubuntu) via WSL
- GoogleTest framework integration
- Unit test coverage for all API functions

⬜ **Planned**:
- [ ] CI testing on native Linux (Ubuntu)
- [ ] Windows (Server 2022) with IOCP implementation
- [ ] macOS with poll() or kqueue()
- [ ] Both 32-bit and 64-bit builds
- [ ] Integration tests with example mudlib

## Error Handling

### Reactor Creation Failure
- **Cause**: System resource exhaustion, invalid parameters
- **Response**: Fatal error during `init_user_conn()`, exit driver

### Handle Registration Failure
- **Cause**: Invalid file descriptor, reactor full, platform limits
- **Response**: Log warning, reject connection gracefully

### Event Wait Failure
- **Cause**: System call interrupted, invalid timeout
- **Response**: Log error, retry with exponential backoff

### Event Processing Errors
- **Cause**: Closed handles, invalid context pointers
- **Response**: Mark connection as `NET_DEAD`, cleanup safely

## Memory Management

### Reactor State
- Allocate once during `init_user_conn()`
- Destroy during `ipc_remove()` or shutdown
- No per-event allocations in hot path

### Event Arrays
- Stack-allocated or static in `do_comm_polling()`/`process_io()`
- Reused across event loop iterations
- Configurable maximum size

### Context Pointers
- Application-managed (reactor only stores/returns them)
- Typically point to `interactive_t`, `port_def_t`, or `lpc_socket_t`
- Must remain valid while handle is registered

## Performance Considerations

### Batching
- Process multiple events per cycle (up to `io_reactor_max_events`)
- Amortize system call overhead

### Zero-Copy
- On platforms supporting it (IOCP), data delivered in event structure
- Avoid redundant buffer copies

### Scalability
- Event loop should be O(1) or O(log n) per event, not O(n)
- Platform implementations must use efficient demultiplexing

## Implementation Phases

### Phase Reports
- ✅ [Phase 1: Core Abstraction](../history/agent-reports/io-reactor-phase1.md) - COMPLETE
- ⬜ Phase 2: Windows IOCP - Not started
- ⬜ Phase 3: Backend Integration - Not started
- ⬜ Phase 4: Future Enhancements - Optional

See detailed phase breakdowns in the respective implementation documents:
- [Linux Implementation](linux-io.md#migration-from-current-code)
- [Windows Implementation](windows-io.md)

## Quick Start

For developers wanting to understand or implement the reactor:

1. Read this document for the platform-agnostic design and abstraction API
2. Read your platform's specific implementation:
   - Linux: [linux-io.md](linux-io.md)
   - Windows: [windows-io.md](windows-io.md)
3. Review test cases in `tests/test_io_reactor/`
4. Check integration points in [src/comm.c](../../src/comm.c)

## Implementation Files

```
lib/port/
├── io_reactor.h           # Platform-agnostic API (this document)
├── io_reactor_poll.c      # POSIX poll() implementation
├── io_reactor_epoll.c     # Linux epoll() implementation (future)
├── io_reactor_kqueue.c    # BSD/macOS kqueue() implementation (future)
└── io_reactor_win32.c     # Windows IOCP implementation
```

## Documentation

### Implementation Reports
- [Phase 1 Report](../history/agent-reports/io-reactor-phase1.md) - Core abstraction complete

### Remaining Documentation Tasks
- [ ] Update [docs/manual/internals.md](internals.md) with reactor architecture
- [ ] Document platform-specific behaviors in [docs/INSTALL.md](../INSTALL.md)
- [ ] Add troubleshooting guide for I/O issues
- [ ] Update [docs/manual/dev.md](dev.md) with portability patterns

## References

- [Reactor Pattern (Douglas Schmidt)](https://www.dre.vanderbilt.edu/~schmidt/PDF/reactor-siemens.pdf)
- [POSA2: Patterns for Concurrent and Networked Objects](https://www.amazon.com/Pattern-Oriented-Software-Architecture-Concurrent-Networked/dp/0471606952)
- Platform-specific references in [linux-io.md](linux-io.md) and [windows-io.md](windows-io.md)

---

**Status**: Draft Design  
**Author**: GitHub Copilot  
**Date**: 2025-12-30  
**Target Version**: Neolith v1.0
