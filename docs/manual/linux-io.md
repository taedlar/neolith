# Linux I/O Reactor Implementation Design

## Overview

This document describes the Linux-specific implementation of the [I/O Reactor abstraction](io-reactor.md) for the Neolith LPMud driver. The Linux implementation leverages POSIX `poll()` (currently) with a future migration path to `epoll()` for better scalability.

## Current Implementation: poll()

Neolith currently uses `poll()` on Linux systems, which provides significant advantages over the legacy `select()` approach:

### Advantages of poll() over select()

1. **No FD_SETSIZE Limit**: Can monitor thousands of file descriptors (limited only by `RLIMIT_NOFILE`)
2. **Cleaner API**: Uses array of `pollfd` structures instead of complex bit manipulation
3. **Simpler Code**: No need to track highest fd number
4. **POSIX Standard**: Available on all modern POSIX systems since 2008

### Current Code Structure

From [src/comm.c](../../src/comm.c):

```c
#ifdef HAVE_POLL
static int total_fds = 0;
static struct pollfd *poll_fds = NULL;
static int console_poll_index = -1;
static int addr_server_poll_index = -1;
#endif

/* In interactive_t structure */
typedef struct interactive_s {
    /* ... */
#ifdef HAVE_POLL
    int poll_index;             /* index in poll_fds[] */
#endif
    /* ... */
} interactive_t;
```

The `poll_index` field tracks each handle's position in the `poll_fds` array, enabling O(1) updates when modifying event masks.

## I/O Reactor Implementation for Linux

### Implementation File: `lib/port/io_reactor_poll.c`

```c
#include "port/io_reactor.h"
#include <poll.h>
#include <stdlib.h>
#include <string.h>

/* Internal reactor state for poll()-based implementation */
typedef struct io_reactor_s {
    struct pollfd *poll_fds;    /* Array of poll file descriptors */
    void **contexts;            /* Parallel array of user context pointers */
    int *fd_to_index;           /* Map fd -> poll_fds index for O(1) lookup */
    int max_fd;                 /* Highest fd seen (for fd_to_index sizing) */
    int num_fds;                /* Number of active fds in poll_fds */
    int capacity;               /* Allocated size of poll_fds/contexts arrays */
} io_reactor_t;

#define INITIAL_CAPACITY 64
#define MAX_FDS 65536           /* Reasonable limit for fd_to_index map */

io_reactor_t* io_reactor_create(void) {
    io_reactor_t *reactor = calloc(1, sizeof(io_reactor_t));
    if (!reactor) {
        return NULL;
    }
    
    reactor->capacity = INITIAL_CAPACITY;
    reactor->poll_fds = calloc(reactor->capacity, sizeof(struct pollfd));
    reactor->contexts = calloc(reactor->capacity, sizeof(void*));
    reactor->fd_to_index = calloc(MAX_FDS, sizeof(int));
    
    if (!reactor->poll_fds || !reactor->contexts || !reactor->fd_to_index) {
        io_reactor_destroy(reactor);
        return NULL;
    }
    
    /* Initialize fd_to_index map to -1 (invalid) */
    memset(reactor->fd_to_index, 0xFF, MAX_FDS * sizeof(int));
    
    reactor->num_fds = 0;
    reactor->max_fd = -1;
    
    return reactor;
}

void io_reactor_destroy(io_reactor_t *reactor) {
    if (!reactor) {
        return;
    }
    
    free(reactor->poll_fds);
    free(reactor->contexts);
    free(reactor->fd_to_index);
    free(reactor);
}

static int ensure_capacity(io_reactor_t *reactor, int min_capacity) {
    if (reactor->capacity >= min_capacity) {
        return 0;
    }
    
    int new_capacity = reactor->capacity;
    while (new_capacity < min_capacity) {
        new_capacity *= 2;
    }
    
    struct pollfd *new_poll_fds = realloc(reactor->poll_fds, 
                                          new_capacity * sizeof(struct pollfd));
    void **new_contexts = realloc(reactor->contexts, 
                                  new_capacity * sizeof(void*));
    
    if (!new_poll_fds || !new_contexts) {
        return -1;
    }
    
    reactor->poll_fds = new_poll_fds;
    reactor->contexts = new_contexts;
    reactor->capacity = new_capacity;
    
    return 0;
}

int io_reactor_add(io_reactor_t *reactor, socket_fd_t fd, void *context, int events) {
    if (!reactor || fd < 0 || fd >= MAX_FDS) {
        return -1;
    }
    
    /* Check if fd already registered */
    if (reactor->fd_to_index[fd] != -1) {
        return -1;  /* Already registered */
    }
    
    /* Ensure capacity */
    if (ensure_capacity(reactor, reactor->num_fds + 1) != 0) {
        return -1;
    }
    
    int index = reactor->num_fds;
    
    /* Setup pollfd structure */
    reactor->poll_fds[index].fd = fd;
    reactor->poll_fds[index].events = 0;
    if (events & EVENT_READ) {
        reactor->poll_fds[index].events |= POLLIN;
    }
    if (events & EVENT_WRITE) {
        reactor->poll_fds[index].events |= POLLOUT;
    }
    reactor->poll_fds[index].revents = 0;
    
    reactor->contexts[index] = context;
    reactor->fd_to_index[fd] = index;
    reactor->num_fds++;
    
    if (fd > reactor->max_fd) {
        reactor->max_fd = fd;
    }
    
    return 0;
}

int io_reactor_modify(io_reactor_t *reactor, socket_fd_t fd, int events) {
    if (!reactor || fd < 0 || fd >= MAX_FDS) {
        return -1;
    }
    
    int index = reactor->fd_to_index[fd];
    if (index == -1) {
        return -1;  /* Not registered */
    }
    
    reactor->poll_fds[index].events = 0;
    if (events & EVENT_READ) {
        reactor->poll_fds[index].events |= POLLIN;
    }
    if (events & EVENT_WRITE) {
        reactor->poll_fds[index].events |= POLLOUT;
    }
    
    return 0;
}

int io_reactor_remove(io_reactor_t *reactor, socket_fd_t fd) {
    if (!reactor || fd < 0 || fd >= MAX_FDS) {
        return -1;
    }
    
    int index = reactor->fd_to_index[fd];
    if (index == -1) {
        return -1;  /* Not registered */
    }
    
    /* Swap with last element to maintain compact array */
    int last_index = reactor->num_fds - 1;
    if (index != last_index) {
        reactor->poll_fds[index] = reactor->poll_fds[last_index];
        reactor->contexts[index] = reactor->contexts[last_index];
        
        /* Update fd_to_index for swapped fd */
        int swapped_fd = reactor->poll_fds[index].fd;
        reactor->fd_to_index[swapped_fd] = index;
    }
    
    reactor->fd_to_index[fd] = -1;
    reactor->num_fds--;
    
    return 0;
}

int io_reactor_wait(io_reactor_t *reactor, io_event_t *events, 
                    int max_events, struct timeval *timeout) {
    if (!reactor || !events || max_events <= 0) {
        return -1;
    }
    
    /* Convert timeout to milliseconds for poll() */
    int timeout_ms;
    if (timeout == NULL) {
        timeout_ms = -1;  /* Block indefinitely */
    } else {
        timeout_ms = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
    }
    
    /* Call poll() */
    int n = poll(reactor->poll_fds, reactor->num_fds, timeout_ms);
    
    if (n <= 0) {
        return n;  /* Error or timeout */
    }
    
    /* Process ready file descriptors */
    int event_count = 0;
    for (int i = 0; i < reactor->num_fds && event_count < max_events; i++) {
        short revents = reactor->poll_fds[i].revents;
        
        if (revents == 0) {
            continue;  /* No events on this fd */
        }
        
        /* Populate event structure */
        events[event_count].context = reactor->contexts[i];
        events[event_count].event_type = 0;
        events[event_count].bytes_transferred = 0;  /* Not used in readiness model */
        events[event_count].buffer = NULL;          /* Not used in readiness model */
        
        if (revents & POLLIN) {
            events[event_count].event_type |= EVENT_READ;
        }
        if (revents & POLLOUT) {
            events[event_count].event_type |= EVENT_WRITE;
        }
        if (revents & (POLLERR | POLLNVAL)) {
            events[event_count].event_type |= EVENT_ERROR;
        }
        if (revents & POLLHUP) {
            events[event_count].event_type |= EVENT_CLOSE;
        }
        
        event_count++;
    }
    
    return event_count;
}

/* No-ops for readiness notification model */
int io_reactor_post_read(io_reactor_t *reactor, socket_fd_t fd, void *buffer, size_t len) {
    /* poll() uses readiness notification, not completion notification */
    /* Reads are performed when EVENT_READ is signaled */
    return 0;
}

int io_reactor_post_write(io_reactor_t *reactor, socket_fd_t fd, void *buffer, size_t len) {
    /* poll() uses readiness notification, not completion notification */
    /* Writes are performed when EVENT_WRITE is signaled */
    return 0;
}
```

## Migration from Current Code

### Step 1: Refactor make_selectmasks()

Current `make_selectmasks()` in [src/comm.c](../../src/comm.c) populates `poll_fds[]`. This logic moves into reactor registration calls:

**Before:**
```c
void make_selectmasks(void) {
    int i_poll = 0;
    
    /* Set fd's for external ports */
    for (i = 0; i < 5; i++) {
        if (external_port[i].port) {
            poll_fds[i_poll].fd = external_port[i].fd;
            poll_fds[i_poll].events = POLLIN;
            external_port[i].poll_index = i_poll;
            i_poll++;
        }
    }
    /* ... more registration ... */
}
```

**After:**
```c
void init_user_conn(void) {
    /* ... socket creation ... */
    
    g_io_reactor = io_reactor_create();
    
    /* Register external ports */
    for (i = 0; i < 5; i++) {
        if (external_port[i].port) {
            io_reactor_add(g_io_reactor, external_port[i].fd, 
                          &external_port[i], EVENT_READ);
        }
    }
}
```

### Step 2: Update new_interactive()

**Before:**
```c
void new_interactive(socket_fd_t socket_fd) {
    /* ... allocate all_users[i] ... */
    
    /* poll_index gets set in make_selectmasks() */
}
```

**After:**
```c
void new_interactive(socket_fd_t socket_fd) {
    /* ... allocate all_users[i] ... */
    
    io_reactor_add(g_io_reactor, socket_fd, all_users[i], EVENT_READ);
}
```

### Step 3: Update remove_interactive()

**Before:**
```c
void remove_interactive(object_t *ob, int dested) {
    /* Removal happens implicitly in make_selectmasks() by checking all_users[] */
    /* ... cleanup ... */
}
```

**After:**
```c
void remove_interactive(object_t *ob, int dested) {
    if (ob->interactive) {
        io_reactor_remove(g_io_reactor, ob->interactive->fd);
    }
    /* ... cleanup ... */
}
```

### Step 4: Simplify process_io()

**Before:**
```c
void process_io(void) {
    for (i = 0; i < max_users; i++) {
        if (USER_CAN_READ(i)) {  /* Macro checks poll_fds[].revents */
            get_user_data(all_users[i]);
        }
    }
}
```

**After:**
```c
void process_io(void) {
    static io_event_t events[MAX_EVENTS];
    int num_events = /* saved from do_comm_polling() */;
    
    for (int i = 0; i < num_events; i++) {
        if (events[i].event_type & EVENT_READ) {
            interactive_t *ip = (interactive_t*)events[i].context;
            get_user_data(ip);
        }
    }
}
```

## Console Input Handling

On Linux, console input (STDIN_FILENO) is handled like any other file descriptor:

```c
void init_console_user(int reconnect) {
    /* ... existing setup ... */
    
    /* Register console with reactor */
    io_reactor_add(g_io_reactor, STDIN_FILENO, master_ob->interactive, EVENT_READ);
}
```

No special handling needed since `poll()` works with any file descriptor.

## Future Enhancement: epoll()

Linux's `epoll()` provides better scalability than `poll()` for large numbers of connections:

### Advantages of epoll()

1. **O(1) Performance**: Only returns ready fds, no scanning
2. **Edge-Triggered Mode**: Can reduce wakeups for high-throughput connections
3. **Kernel-Side State**: No need to copy entire fd set on each call

### Migration Path

1. Implement `lib/port/io_reactor_epoll.c` following same API
2. CMake detects `epoll_create1()` availability
3. Select implementation at build time:

```cmake
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    check_symbol_exists(epoll_create1 "sys/epoll.h" HAVE_EPOLL)
    if(HAVE_EPOLL)
        target_sources(port PRIVATE io_reactor_epoll.c)
    else()
        target_sources(port PRIVATE io_reactor_poll.c)
    endif()
else()
    target_sources(port PRIVATE io_reactor_poll.c)
endif()
```

### epoll() Implementation Sketch

```c
typedef struct io_reactor_s {
    int epoll_fd;
    void **contexts;            /* Map fd -> context */
    int max_fd;
} io_reactor_t;

io_reactor_t* io_reactor_create(void) {
    io_reactor_t *reactor = calloc(1, sizeof(io_reactor_t));
    
    reactor->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (reactor->epoll_fd < 0) {
        free(reactor);
        return NULL;
    }
    
    reactor->contexts = calloc(MAX_FDS, sizeof(void*));
    return reactor;
}

int io_reactor_add(io_reactor_t *reactor, socket_fd_t fd, void *context, int events) {
    struct epoll_event ev;
    ev.events = 0;
    if (events & EVENT_READ)  ev.events |= EPOLLIN;
    if (events & EVENT_WRITE) ev.events |= EPOLLOUT;
    ev.data.fd = fd;
    
    if (epoll_ctl(reactor->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        return -1;
    }
    
    reactor->contexts[fd] = context;
    return 0;
}

int io_reactor_wait(io_reactor_t *reactor, io_event_t *events, 
                    int max_events, struct timeval *timeout) {
    struct epoll_event ep_events[max_events];
    int timeout_ms = timeout ? (timeout->tv_sec * 1000 + timeout->tv_usec / 1000) : -1;
    
    int n = epoll_wait(reactor->epoll_fd, ep_events, max_events, timeout_ms);
    if (n <= 0) return n;
    
    for (int i = 0; i < n; i++) {
        int fd = ep_events[i].data.fd;
        events[i].context = reactor->contexts[fd];
        events[i].event_type = 0;
        
        if (ep_events[i].events & EPOLLIN)  events[i].event_type |= EVENT_READ;
        if (ep_events[i].events & EPOLLOUT) events[i].event_type |= EVENT_WRITE;
        if (ep_events[i].events & EPOLLERR) events[i].event_type |= EVENT_ERROR;
        if (ep_events[i].events & EPOLLHUP) events[i].event_type |= EVENT_CLOSE;
    }
    
    return n;
}
```

## Performance Characteristics

### poll() Performance

| Metric                  | Performance         | Notes                           |
|-------------------------|---------------------|---------------------------------|
| Max Connections         | 10,000+             | Limited by `RLIMIT_NOFILE`      |
| Readiness Check         | O(n)                | Scans all registered fds        |
| Memory per Connection   | 8 bytes (pollfd)    | Plus context pointer            |
| Typical Overhead        | ~1-2μs per fd       | Modern kernels optimize well    |

### epoll() Performance (Future)

| Metric                  | Performance         | Notes                           |
|-------------------------|---------------------|---------------------------------|
| Max Connections         | 100,000+            | Production-tested               |
| Readiness Check         | O(1)                | Only returns ready fds          |
| Memory per Connection   | ~128 bytes          | Kernel-side event queue         |
| Typical Overhead        | ~0.5μs per event    | Significantly faster at scale   |

### Benchmark Comparison

Expected performance at different scales:

```
100 connections:   poll() ≈ epoll() (minimal difference)
1,000 connections: poll() ~2ms, epoll() ~0.5ms per cycle
10,000 connections: poll() ~20ms, epoll() ~0.5ms per cycle
```

For typical MUD workloads (<500 connections), `poll()` is sufficient.

## Testing

### Unit Tests: `tests/test_io_reactor/test_poll.cpp`

```cpp
TEST(IOReactorPollTest, BasicReadiness) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    int pipe_fds[2];
    ASSERT_EQ(0, pipe(pipe_fds));
    
    // Register read end
    io_reactor_add(reactor, pipe_fds[0], (void*)0xDEADBEEF, EVENT_READ);
    
    // Write to pipe
    write(pipe_fds[1], "test", 4);
    
    // Should become readable
    io_event_t events[10];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    EXPECT_EQ(1, n);
    EXPECT_EQ(EVENT_READ, events[0].event_type);
    EXPECT_EQ((void*)0xDEADBEEF, events[0].context);
    
    // Cleanup
    io_reactor_remove(reactor, pipe_fds[0]);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    io_reactor_destroy(reactor);
}

TEST(IOReactorPollTest, ScalabilityStress) {
    io_reactor_t* reactor = io_reactor_create();
    
    const int NUM_SOCKETS = 1000;
    int sockets[NUM_SOCKETS];
    
    // Create and register many sockets
    for (int i = 0; i < NUM_SOCKETS; i++) {
        sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_GE(sockets[i], 0);
        EXPECT_EQ(0, io_reactor_add(reactor, sockets[i], (void*)(intptr_t)i, EVENT_READ));
    }
    
    // Verify timeout works
    io_event_t events[100];
    struct timeval timeout = {0, 10000};  // 10ms
    int n = io_reactor_wait(reactor, events, 100, &timeout);
    EXPECT_EQ(0, n);  // No events
    
    // Cleanup
    for (int i = 0; i < NUM_SOCKETS; i++) {
        io_reactor_remove(reactor, sockets[i]);
        close(sockets[i]);
    }
    io_reactor_destroy(reactor);
}
```

## Configuration

No Linux-specific configuration needed beyond generic reactor settings in [io-reactor.md](io-reactor.md).

## Error Handling

### Common Errors

1. **EINTR**: System call interrupted by signal
   - **Response**: Retry `poll()` automatically
   
2. **ENOMEM**: Out of memory
   - **Response**: Fatal error, cannot continue
   
3. **EINVAL**: Invalid parameters (negative timeout, bad nfds)
   - **Response**: Log error, treat as timeout

4. **POLLHUP**: Peer closed connection
   - **Response**: Deliver EVENT_CLOSE to handler

5. **POLLNVAL**: Invalid fd (not open)
   - **Response**: Remove from reactor, log warning

## References

- [poll() Man Page](https://man7.org/linux/man-pages/man2/poll.2.html)
- [epoll() Man Page](https://man7.org/linux/man-pages/man7/epoll.7.html)
- [The C10K Problem](http://www.kegel.com/c10k.html)
- [epoll() Tutorial](https://man7.org/linux/man-pages/man7/epoll.7.html#EXAMPLES)

---

**Status**: Draft Design  
**Author**: GitHub Copilot  
**Date**: 2025-12-30  
**Target Version**: Neolith v1.0
