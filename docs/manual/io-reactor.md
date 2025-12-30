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

### Current State

- **Linux/POSIX**: Uses `poll()` for scalable I/O multiplexing
- **Windows**: Uses `select()` with FD_SETSIZE limitations and special console handling

## Reactor Pattern Fundamentals

The reactor pattern is a design pattern for handling service requests delivered concurrently to an application by demultiplexing events and dispatching them to appropriate handlers.

### Key Components

1. **Handles**: File descriptors, sockets, or other I/O sources
2. **Event Demultiplexer**: Platform-specific mechanism (poll, epoll, IOCP, kqueue)
3. **Event Handlers**: Callbacks that process specific event types
4. **Reactor**: Orchestrates event detection and handler dispatch

### Flow

```
┌─────────────────────────────────────────────────────────────┐
│                      Backend Loop                            │
│                                                               │
│  ┌────────────┐     ┌──────────────┐    ┌─────────────┐    │
│  │  Platform  │────>│  I/O Reactor │───>│  Event      │    │
│  │  Detector  │     │   Abstraction│    │  Handlers   │    │
│  └────────────┘     └──────────────┘    └─────────────┘    │
│                            │                                 │
│                            ├──── Linux: poll()/epoll()      │
│                            │                                 │
│                            └──── Windows: IOCP              │
└─────────────────────────────────────────────────────────────┘
```

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

### Header: `lib/port/io_reactor.h`

```c
#pragma once

#include "port/socket_comm.h"
#include <sys/time.h>

/* Event types */
#define EVENT_READ   0x01
#define EVENT_WRITE  0x02
#define EVENT_ERROR  0x04
#define EVENT_CLOSE  0x08

/* Event structure returned by io_reactor_wait() */
typedef struct io_event_s {
    void *context;              /* Associated object (interactive_t*, port_def_t*, etc.) */
    int event_type;             /* EVENT_READ, EVENT_WRITE, EVENT_ERROR, EVENT_CLOSE */
    int bytes_transferred;      /* For async operations (platform-specific) */
    void *buffer;               /* Buffer for pending I/O (platform-specific) */
} io_event_t;

/* Opaque reactor handle */
typedef struct io_reactor_s io_reactor_t;

/*
 * Lifecycle Management
 */

/**
 * @brief Create a new I/O reactor instance.
 * @return Pointer to reactor, or NULL on failure.
 */
io_reactor_t* io_reactor_create(void);

/**
 * @brief Destroy an I/O reactor and release all resources.
 * @param reactor The reactor to destroy.
 */
void io_reactor_destroy(io_reactor_t *reactor);

/*
 * Handle Registration
 */

/**
 * @brief Register a file descriptor/socket with the reactor.
 * @param reactor The reactor instance.
 * @param fd The file descriptor to monitor.
 * @param context User context pointer (stored and returned with events).
 * @param events Bitmask of events to monitor (EVENT_READ | EVENT_WRITE).
 * @return 0 on success, -1 on failure.
 */
int io_reactor_add(io_reactor_t *reactor, socket_fd_t fd, void *context, int events);

/**
 * @brief Modify the event mask for a registered file descriptor.
 * @param reactor The reactor instance.
 * @param fd The file descriptor to modify.
 * @param events New event mask (EVENT_READ | EVENT_WRITE).
 * @return 0 on success, -1 on failure.
 */
int io_reactor_modify(io_reactor_t *reactor, socket_fd_t fd, int events);

/**
 * @brief Unregister a file descriptor from the reactor.
 * @param reactor The reactor instance.
 * @param fd The file descriptor to remove.
 * @return 0 on success, -1 on failure.
 */
int io_reactor_remove(io_reactor_t *reactor, socket_fd_t fd);

/*
 * Event Loop Integration
 */

/**
 * @brief Wait for I/O events and return them.
 * @param reactor The reactor instance.
 * @param events Array to store returned events.
 * @param max_events Maximum number of events to return.
 * @param timeout Timeout for waiting (NULL = block indefinitely).
 * @return Number of events returned (>= 0), or -1 on error.
 *
 * This is the core event demultiplexing function. It blocks until:
 * - One or more events occur
 * - The timeout expires
 * - An error occurs
 *
 * On platforms with async I/O (Windows IOCP), this returns completion events.
 * On platforms with readiness notification (Linux poll), this returns ready handles.
 */
int io_reactor_wait(io_reactor_t *reactor, io_event_t *events, 
                    int max_events, struct timeval *timeout);

/*
 * Platform-Specific Helpers
 *
 * These functions abstract platform differences in I/O models:
 * - On POSIX systems with readiness notification, they may be no-ops
 * - On Windows with completion notification (IOCP), they post async operations
 */

/**
 * @brief Post an asynchronous read operation (platform-specific).
 * @param reactor The reactor instance.
 * @param fd The file descriptor to read from.
 * @param buffer Buffer to read into (NULL = use internal buffer).
 * @param len Size of buffer.
 * @return 0 on success, -1 on failure.
 *
 * On POSIX: This is typically a no-op (reads happen when EVENT_READ fires).
 * On Windows IOCP: Posts an async WSARecv() operation.
 */
int io_reactor_post_read(io_reactor_t *reactor, socket_fd_t fd, void *buffer, size_t len);

/**
 * @brief Post an asynchronous write operation (platform-specific).
 * @param reactor The reactor instance.
 * @param fd The file descriptor to write to.
 * @param buffer Buffer containing data to write.
 * @param len Number of bytes to write.
 * @return 0 on success, -1 on failure.
 *
 * On POSIX: This is typically a no-op (writes happen when EVENT_WRITE fires).
 * On Windows IOCP: Posts an async WSASend() operation.
 */
int io_reactor_post_write(io_reactor_t *reactor, socket_fd_t fd, void *buffer, size_t len);
```

## Integration with Neolith Backend

### Initialization

In [src/comm.c](../../src/comm.c), replace direct `poll()`/`select()` usage:

```c
#include "port/io_reactor.h"

static io_reactor_t *g_io_reactor = NULL;

void init_user_conn(void) {
    /* ... existing socket setup ... */
    
    g_io_reactor = io_reactor_create();
    if (!g_io_reactor) {
        debug_fatal("Failed to create I/O reactor\n");
        exit(EXIT_FAILURE);
    }
    
    /* Add console if in console mode */
    if (MAIN_OPTION(console_mode)) {
        /* Platform-specific console registration */
        io_reactor_add_console(g_io_reactor);
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

### Event Loop

Replace `do_comm_polling()` to use reactor:

```c
int do_comm_polling(struct timeval *timeout) {
    static io_event_t events[MAX_EVENTS];
    
    opt_trace(TT_BACKEND|3, "do_comm_polling: timeout %ld sec, %ld usec",
              timeout->tv_sec, timeout->tv_usec);
    
    return io_reactor_wait(g_io_reactor, events, MAX_EVENTS, timeout);
}
```

### Handle Registration

Register new interactive users:

```c
void new_interactive(socket_fd_t socket_fd) {
    /* ... existing code to allocate all_users[i] ... */
    
    /* Register with I/O reactor */
    if (io_reactor_add(g_io_reactor, socket_fd, all_users[i], EVENT_READ) != 0) {
        debug_message("Failed to register socket %d with I/O reactor\n", socket_fd);
        /* Handle error */
    }
}
```

Unregister on disconnect:

```c
void remove_interactive(object_t *ob, int dested) {
    if (ob->interactive) {
        io_reactor_remove(g_io_reactor, ob->interactive->fd);
    }
    
    /* ... existing cleanup ... */
}
```

### Event Processing

Modify `process_io()` to handle reactor events:

```c
void process_io(void) {
    static io_event_t events[MAX_EVENTS];
    int num_events = /* saved from last do_comm_polling() call */;
    
    for (int i = 0; i < num_events; i++) {
        io_event_t *evt = &events[i];
        
        switch (evt->event_type) {
            case EVENT_READ: {
                /* Handle read event */
                interactive_t *ip = (interactive_t*)evt->context;
                
                /* On completion-based platforms, data is in evt->buffer */
                /* On readiness-based platforms, call get_user_data() */
                if (evt->buffer && evt->bytes_transferred > 0) {
                    /* Process pre-read data (IOCP) */
                    process_input_buffer(ip, evt->buffer, evt->bytes_transferred);
                    /* Repost async read */
                    io_reactor_post_read(g_io_reactor, ip->fd, NULL, 0);
                } else {
                    /* Readiness notification (poll) */
                    get_user_data(ip);
                }
                break;
            }
            
            case EVENT_WRITE: {
                /* Handle write completion */
                interactive_t *ip = (interactive_t*)evt->context;
                flush_message(ip);
                break;
            }
            
            case EVENT_CLOSE: {
                /* Handle disconnection */
                interactive_t *ip = (interactive_t*)evt->context;
                remove_interactive(ip->ob, 0);
                break;
            }
            
            case EVENT_ERROR: {
                /* Handle I/O error */
                interactive_t *ip = (interactive_t*)evt->context;
                ip->iflags |= NET_DEAD;
                break;
            }
        }
    }
}
```

## Platform Implementation Requirements

Each platform must implement the reactor interface defined above. See platform-specific documents:

- [Linux I/O Reactor Design](linux-io.md) - Using `poll()` or `epoll()`
- [Windows I/O Reactor Design](windows-io.md) - Using I/O Completion Ports (IOCP)

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

Platform-specific implementation selected via CMake:

```cmake
# lib/port/CMakeLists.txt
if(WIN32)
    target_sources(port PRIVATE io_reactor_win32.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(port PRIVATE io_reactor_epoll.c)  # Or io_reactor_poll.c
else()
    target_sources(port PRIVATE io_reactor_poll.c)   # Fallback to POSIX poll()
endif()
```

## Testing Strategy

### Unit Tests: `tests/test_io_reactor/`

Create GoogleTest suite for reactor abstraction:

```cpp
TEST(IOReactorTest, CreateDestroy) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    io_reactor_destroy(reactor);
}

TEST(IOReactorTest, AddRemoveSocket) {
    io_reactor_t* reactor = io_reactor_create();
    socket_fd_t fd = create_test_socket();
    
    EXPECT_EQ(0, io_reactor_add(reactor, fd, nullptr, EVENT_READ));
    EXPECT_EQ(0, io_reactor_remove(reactor, fd));
    
    close_test_socket(fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorTest, EventDelivery) {
    io_reactor_t* reactor = io_reactor_create();
    socket_fd_t server_fd, client_fd;
    create_socket_pair(&server_fd, &client_fd);
    
    io_reactor_add(reactor, server_fd, (void*)0x1234, EVENT_READ);
    
    // Write to client side
    write(client_fd, "test", 4);
    
    // Should get read event on server side
    io_event_t events[10];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    EXPECT_EQ(1, n);
    EXPECT_EQ(EVENT_READ, events[0].event_type);
    EXPECT_EQ((void*)0x1234, events[0].context);
    
    // Cleanup
    io_reactor_remove(reactor, server_fd);
    close_socket_pair(server_fd, client_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorTest, Timeout) {
    io_reactor_t* reactor = io_reactor_create();
    io_event_t events[10];
    struct timeval timeout = {0, 100000};  // 100ms
    
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    EXPECT_EQ(0, n);  // Should timeout with no events
    
    io_reactor_destroy(reactor);
}
```

### Integration Tests

Use example mudlib with stress testing:

1. **Multi-player Load**: 100+ concurrent connections
2. **Console Mode**: Verify STDIN handling works
3. **Message Throughput**: Large messages, rapid sends
4. **Connection Churn**: Rapid connect/disconnect cycles

### Platform Coverage

- CI testing on Linux (Ubuntu), Windows (Server 2022), macOS
- Both 32-bit and 64-bit builds
- Test with actual mudlib, not just unit tests

## Configuration Options

Add to [src/neolith.conf](../../src/neolith.conf):

```
# I/O Reactor Configuration

# Maximum events to process per cycle
io_reactor_max_events : 128

# Enable I/O reactor tracing
trace io_reactor : 0
```

Add trace flag in [src/stem.h](../../src/stem.h):

```c
#define TT_IO_REACTOR      0x00200000  /* I/O reactor operations */
```

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

See detailed phase breakdowns in the respective implementation documents:
- [Linux Implementation](linux-io.md#migration-from-current-code)
- [Windows Implementation](windows-io.md)

### High-Level Roadmap

**Phase 1: Core Abstraction**
- Define reactor API in `io_reactor.h`
- Implement Linux `poll()` backend
- Basic unit tests

**Phase 2: Windows IOCP**
- Implement Windows IOCP backend
- Console support for both platforms
- Integration tests

**Phase 3: Production Readiness**
- Performance optimization
- Error handling refinement
- Documentation
- CI/CD integration

**Phase 4: Future Enhancements** (Optional)
- Linux `epoll()` backend
- Windows thread pool integration
- Advanced features (AcceptEx, zero-copy I/O)

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

## Documentation Updates

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
