/**
 * @file async_runtime_epoll.c
 * @brief Linux epoll-based async runtime implementation
 * 
 * Uses epoll for efficient I/O multiplexing and eventfd for worker completion notifications.
 */

#if defined(__linux__)

#include "async/async_runtime.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MAX_EVENTS 64

struct async_runtime_s {
    int epoll_fd;
    int event_fd;  /* For worker completions */
};

/* Helper functions */

static uint32_t events_to_epoll(uint32_t events) {
    uint32_t epoll_events = 0;
    if (events & EVENT_READ) epoll_events |= EPOLLIN;
    if (events & EVENT_WRITE) epoll_events |= EPOLLOUT;
    return epoll_events;
}

static uint32_t epoll_to_events(uint32_t epoll_events) {
    uint32_t events = 0;
    if (epoll_events & EPOLLIN) events |= EVENT_READ;
    if (epoll_events & EPOLLOUT) events |= EVENT_WRITE;
    if (epoll_events & EPOLLERR) events |= EVENT_ERROR;
    if (epoll_events & EPOLLHUP) events |= EVENT_CLOSE;
    return events;
}

/* Public API */

async_runtime_t* async_runtime_init(void) {
    async_runtime_t* runtime = calloc(1, sizeof(async_runtime_t));
    if (!runtime) return NULL;
    
    runtime->epoll_fd = epoll_create1(0);
    if (runtime->epoll_fd < 0) {
        free(runtime);
        return NULL;
    }
    
    /* Create eventfd for worker notifications */
    runtime->event_fd = eventfd(0, EFD_NONBLOCK);
    if (runtime->event_fd < 0) {
        close(runtime->epoll_fd);
        free(runtime);
        return NULL;
    }
    
    /* Add eventfd to epoll */
    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.fd = runtime->event_fd;
    if (epoll_ctl(runtime->epoll_fd, EPOLL_CTL_ADD, runtime->event_fd, &ev) < 0) {
        close(runtime->event_fd);
        close(runtime->epoll_fd);
        free(runtime);
        return NULL;
    }
    
    return runtime;
}

void async_runtime_deinit(async_runtime_t* runtime) {
    if (!runtime) return;
    
    if (runtime->event_fd >= 0) {
        close(runtime->event_fd);
    }
    
    if (runtime->epoll_fd >= 0) {
        close(runtime->epoll_fd);
    }
    
    free(runtime);
}

int async_runtime_add(async_runtime_t* runtime, socket_fd_t fd, uint32_t events, void* context) {
    if (!runtime || fd < 0) return -1;
    
    struct epoll_event ev = {0};
    ev.events = events_to_epoll(events);
    ev.data.ptr = context;
    
    return epoll_ctl(runtime->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

int async_runtime_modify(async_runtime_t* runtime, socket_fd_t fd, uint32_t events) {
    if (!runtime || fd < 0) return -1;
    
    struct epoll_event ev = {0};
    ev.events = events_to_epoll(events);
    
    return epoll_ctl(runtime->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

int async_runtime_remove(async_runtime_t* runtime, socket_fd_t fd) {
    if (!runtime || fd < 0) return -1;
    
    return epoll_ctl(runtime->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

int async_runtime_wakeup(async_runtime_t* runtime) {
    if (!runtime || runtime->event_fd < 0) return -1;
    
    uint64_t val = 1;
    ssize_t n = write(runtime->event_fd, &val, sizeof(val));
    return (n == sizeof(val)) ? 0 : -1;
}

int async_runtime_wait(async_runtime_t* runtime, io_event_t* events,
                       int max_events, struct timeval* timeout) {
    if (!runtime || !events || max_events <= 0) return -1;
    
    int timeout_ms = -1;
    if (timeout) {
        timeout_ms = (timeout->tv_sec * 1000) + (timeout->tv_usec / 1000);
    }
    
    struct epoll_event epoll_events[MAX_EVENTS];
    int max_epoll_events = (max_events < MAX_EVENTS) ? max_events : MAX_EVENTS;
    
    int result = epoll_wait(runtime->epoll_fd, epoll_events, max_epoll_events, timeout_ms);
    if (result < 0) {
        /* EINTR (signal interruption) is used to wake up the event loop.
         * Treat it as timeout so backend can check heartbeat/shutdown flags. */
        return (errno == EINTR) ? 0 : -1;
    }
    if (result == 0) return 0;  /* Timeout */
    
    int event_count = 0;
    for (int i = 0; i < result && event_count < max_events; i++) {
        /* Check if this is the eventfd */
        if (epoll_events[i].data.fd == runtime->event_fd) {
            /* Drain eventfd and decode worker completions */
            uint64_t val;
            while (read(runtime->event_fd, &val, sizeof(val)) == sizeof(val)) {
                if (event_count < max_events) {
                    events[event_count].fd = -1;
                    events[event_count].completion_key = (uintptr_t)(val >> 32);
                    events[event_count].context = NULL;
                    events[event_count].event_type = EVENT_READ;
                    events[event_count].bytes_transferred = (int)(val & 0xFFFFFFFF);
                    events[event_count].buffer = NULL;
                    event_count++;
                }
            }
        } else {
            /* Regular I/O event */
            events[event_count].fd = epoll_events[i].data.fd;
            events[event_count].completion_key = 0;
            events[event_count].context = epoll_events[i].data.ptr;
            events[event_count].event_type = epoll_to_events(epoll_events[i].events);
            events[event_count].bytes_transferred = 0;
            events[event_count].buffer = NULL;
            event_count++;
        }
    }
    
    return event_count;
}

int async_runtime_post_completion(async_runtime_t* runtime, uintptr_t completion_key, uintptr_t data) {
    if (!runtime || runtime->event_fd < 0) return -1;
    
    /* Write to eventfd to wake up epoll_wait */
    uint64_t val = (((uint64_t)completion_key) << 32) | (data & 0xFFFFFFFF);
    ssize_t n = write(runtime->event_fd, &val, sizeof(val));
    
    return (n == sizeof(val)) ? 0 : -1;
}

int async_runtime_post_read(async_runtime_t* runtime, socket_fd_t fd, void* buffer, size_t len) {
    /* No-op on epoll (readiness-based) */
    (void)runtime; (void)fd; (void)buffer; (void)len;
    return 0;
}

int async_runtime_post_write(async_runtime_t* runtime, socket_fd_t fd, void* buffer, size_t len) {
    /* No-op on epoll (readiness-based) */
    (void)runtime; (void)fd; (void)buffer; (void)len;
    return 0;
}

int async_runtime_get_event_loop_handle(async_runtime_t* runtime) {
    return runtime ? runtime->event_fd : -1;
}

#endif /* __linux__ */
