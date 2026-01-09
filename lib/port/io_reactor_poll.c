/**
 * @file io_reactor_poll.c
 * @brief POSIX poll()-based I/O reactor implementation.
 *
 * This implementation uses poll() for event demultiplexing on POSIX systems.
 * It provides readiness notification: poll() returns when file descriptors
 * are ready for I/O, and the application performs the actual read/write.
 *
 * Platform: Linux, BSD, macOS, other POSIX-compliant systems
 * Performance: O(n) scan of registered descriptors
 * Scalability: Good for <1000 connections
 *
 * Future enhancement: io_reactor_epoll.c using epoll() for O(1) scalability.
 */

#include "io_reactor.h"
#include "socket_comm.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

/* Initial capacity for pollfd array */
#define INITIAL_CAPACITY 64

/* Maximum number of file descriptors to track */
#define MAX_FD_COUNT 4096

/**
 * @brief Mapping entry from file descriptor to context and event mask.
 *
 * We maintain a separate mapping array to associate each pollfd with its
 * user context pointer. This allows O(1) context lookup when events occur.
 */
typedef struct fd_mapping_s {
    socket_fd_t fd;        /* File descriptor (-1 if slot is unused) */
    void *context;         /* User-supplied context pointer */
    int events;            /* Requested event mask (EVENT_READ | EVENT_WRITE) */
} fd_mapping_t;

/**
 * @brief POSIX poll()-based reactor implementation.
 */
struct io_reactor_s {
    struct pollfd *pollfds;     /* Array of pollfd structures for poll() */
    fd_mapping_t *mappings;     /* Parallel array mapping fd -> context */
    int capacity;               /* Allocated size of pollfds/mappings arrays */
    int count;                  /* Number of registered file descriptors */
};

/*
 * =============================================================================
 * Helper Functions
 * =============================================================================
 */

/**
 * @brief Find the index of a file descriptor in the reactor's arrays.
 * @param reactor The reactor instance.
 * @param fd The file descriptor to find.
 * @return Index in pollfds/mappings arrays, or -1 if not found.
 */
static int find_fd_index(io_reactor_t *reactor, socket_fd_t fd) {
    for (int i = 0; i < reactor->count; i++) {
        if (reactor->pollfds[i].fd == fd) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Convert EVENT_* flags to poll() POLL* flags.
 * @param events EVENT_READ | EVENT_WRITE bitmask.
 * @return Corresponding POLLIN | POLLOUT bitmask.
 */
static short events_to_poll(int events) {
    short poll_events = 0;
    
    if (events & EVENT_READ) {
        poll_events |= POLLIN;
    }
    if (events & EVENT_WRITE) {
        poll_events |= POLLOUT;
    }
    
    return poll_events;
}

/**
 * @brief Convert poll() POLL* flags to EVENT_* flags.
 * @param revents Poll return events (POLLIN, POLLOUT, POLLERR, POLLHUP, etc.).
 * @return Corresponding EVENT_* bitmask.
 */
static int poll_to_events(short revents) {
    int events = 0;
    
    if (revents & POLLIN) {
        events |= EVENT_READ;
    }
    if (revents & POLLOUT) {
        events |= EVENT_WRITE;
    }
    if (revents & POLLERR) {
        events |= EVENT_ERROR;
    }
    if (revents & (POLLHUP | POLLNVAL)) {
        events |= EVENT_CLOSE;
    }
    
    return events;
}

/**
 * @brief Expand the reactor's internal arrays.
 * @param reactor The reactor instance.
 * @return 0 on success, -1 on allocation failure.
 */
static int expand_capacity(io_reactor_t *reactor) {
    int new_capacity = reactor->capacity * 2;
    if (new_capacity > MAX_FD_COUNT) {
        new_capacity = MAX_FD_COUNT;
    }
    
    /* Reallocate pollfd array */
    struct pollfd *new_pollfds = realloc(reactor->pollfds, 
                                          new_capacity * sizeof(struct pollfd));
    if (!new_pollfds) {
        return -1;
    }
    reactor->pollfds = new_pollfds;
    
    /* Reallocate mapping array */
    fd_mapping_t *new_mappings = realloc(reactor->mappings,
                                          new_capacity * sizeof(fd_mapping_t));
    if (!new_mappings) {
        return -1;
    }
    reactor->mappings = new_mappings;
    
    /* Initialize new slots */
    for (int i = reactor->capacity; i < new_capacity; i++) {
        reactor->pollfds[i].fd = -1;
        reactor->pollfds[i].events = 0;
        reactor->pollfds[i].revents = 0;
        reactor->mappings[i].fd = -1;
        reactor->mappings[i].context = NULL;
        reactor->mappings[i].events = 0;
    }
    
    reactor->capacity = new_capacity;
    return 0;
}

/*
 * =============================================================================
 * Public API Implementation
 * =============================================================================
 */

io_reactor_t* io_reactor_create(void) {
    io_reactor_t *reactor = malloc(sizeof(io_reactor_t));
    if (!reactor) {
        return NULL;
    }
    
    /* Allocate initial arrays */
    reactor->pollfds = malloc(INITIAL_CAPACITY * sizeof(struct pollfd));
    reactor->mappings = malloc(INITIAL_CAPACITY * sizeof(fd_mapping_t));
    
    if (!reactor->pollfds || !reactor->mappings) {
        free(reactor->pollfds);
        free(reactor->mappings);
        free(reactor);
        return NULL;
    }
    
    reactor->capacity = INITIAL_CAPACITY;
    reactor->count = 0;
    
    /* Initialize all slots */
    for (int i = 0; i < INITIAL_CAPACITY; i++) {
        reactor->pollfds[i].fd = -1;
        reactor->pollfds[i].events = 0;
        reactor->pollfds[i].revents = 0;
        reactor->mappings[i].fd = -1;
        reactor->mappings[i].context = NULL;
        reactor->mappings[i].events = 0;
    }
    
    return reactor;
}

void io_reactor_destroy(io_reactor_t *reactor) {
    if (!reactor) {
        return;
    }
    
    free(reactor->pollfds);
    free(reactor->mappings);
    free(reactor);
}

int io_reactor_add(io_reactor_t *reactor, socket_fd_t fd, void *context, int events) {
    if (!reactor || fd < 0) {
        return -1;
    }
    
    /* Check if already registered */
    if (find_fd_index(reactor, fd) >= 0) {
        errno = EEXIST;
        return -1;
    }
    
    /* Expand capacity if needed */
    if (reactor->count >= reactor->capacity) {
        if (expand_capacity(reactor) != 0) {
            return -1;
        }
    }
    
    /* Add to next available slot */
    int idx = reactor->count;
    reactor->pollfds[idx].fd = fd;
    reactor->pollfds[idx].events = events_to_poll(events);
    reactor->pollfds[idx].revents = 0;
    
    reactor->mappings[idx].fd = fd;
    reactor->mappings[idx].context = context;
    reactor->mappings[idx].events = events;
    
    reactor->count++;
    return 0;
}

int io_reactor_modify(io_reactor_t *reactor, socket_fd_t fd, int events) {
    if (!reactor || fd < 0) {
        return -1;
    }
    
    int idx = find_fd_index(reactor, fd);
    if (idx < 0) {
        errno = ENOENT;
        return -1;
    }
    
    reactor->pollfds[idx].events = events_to_poll(events);
    reactor->mappings[idx].events = events;
    
    return 0;
}

int io_reactor_remove(io_reactor_t *reactor, socket_fd_t fd) {
    if (!reactor || fd < 0) {
        return -1;
    }
    
    int idx = find_fd_index(reactor, fd);
    if (idx < 0) {
        /* Not found - not an error, just a no-op */
        return 0;
    }
    
    /* Move last element to this position (swap-and-pop) */
    int last_idx = reactor->count - 1;
    if (idx != last_idx) {
        reactor->pollfds[idx] = reactor->pollfds[last_idx];
        reactor->mappings[idx] = reactor->mappings[last_idx];
    }
    
    /* Clear the last slot */
    reactor->pollfds[last_idx].fd = -1;
    reactor->pollfds[last_idx].events = 0;
    reactor->pollfds[last_idx].revents = 0;
    reactor->mappings[last_idx].fd = -1;
    reactor->mappings[last_idx].context = NULL;
    reactor->mappings[last_idx].events = 0;
    
    reactor->count--;
    return 0;
}

int io_reactor_wait(io_reactor_t *reactor, io_event_t *events,
                    int max_events, struct timeval *timeout) {
    if (!reactor || !events || max_events <= 0) {
        return -1;
    }
    
    /* Convert timeval to milliseconds for poll() */
    int timeout_ms;
    if (timeout == NULL) {
        timeout_ms = -1;  /* Block indefinitely */
    } else {
        timeout_ms = (timeout->tv_sec * 1000) + (timeout->tv_usec / 1000);
    }
    
    /* Call poll() to wait for events */
    int ready = poll(reactor->pollfds, reactor->count, timeout_ms);
    
    if (ready < 0) {
        /* Error occurred */
        if (errno == EINTR) {
            /* Interrupted by signal - return 0 events */
            return 0;
        }
        return -1;
    }
    
    if (ready == 0) {
        /* Timeout - no events */
        return 0;
    }
    
    /* Collect events from ready file descriptors */
    int event_count = 0;
    for (int i = 0; i < reactor->count && event_count < max_events && ready > 0; i++) {
        if (reactor->pollfds[i].revents == 0) {
            continue;  /* No events on this fd */
        }
        
        /* Convert poll revents to our event format */
        events[event_count].context = reactor->mappings[i].context;
        events[event_count].event_type = poll_to_events(reactor->pollfds[i].revents);
        events[event_count].bytes_transferred = 0;  /* Not used in readiness model */
        events[event_count].buffer = NULL;          /* Not used in readiness model */
        
        event_count++;
        ready--;  /* Decrement ready count for early exit optimization */
    }
    
    return event_count;
}

/*
 * Platform-specific helpers: No-ops for poll()-based implementation.
 * These functions are primarily for Windows IOCP compatibility.
 */

int io_reactor_post_read(io_reactor_t *reactor, socket_fd_t fd, void *buffer, size_t len) {
    (void)reactor;
    (void)fd;
    (void)buffer;
    (void)len;
    /* No-op: In readiness notification model, reads happen after EVENT_READ */
    return 0;
}

int io_reactor_post_write(io_reactor_t *reactor, socket_fd_t fd, void *buffer, size_t len) {
    (void)reactor;
    (void)fd;
    (void)buffer;
    (void)len;
    /* No-op: In readiness notification model, writes happen after EVENT_WRITE */
    return 0;
}

int io_reactor_wakeup(io_reactor_t *reactor) {
    (void)reactor;
    /* No-op: On POSIX, signals (e.g., SIGALRM) automatically interrupt
     * poll()/epoll_wait() with EINTR. Explicit wakeup is not needed. */
    return 0;
}
