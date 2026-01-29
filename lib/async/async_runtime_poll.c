/**
 * @file async_runtime_poll.c
 * @brief Fallback poll-based async runtime implementation
 * 
 * Uses poll() for I/O multiplexing and pipe for worker completion notifications.
 * Suitable for BSD, macOS, and other POSIX systems without epoll.
 */

#if !defined(_WIN32) && !defined(__linux__)

#include "async/async_runtime.h"
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define INITIAL_CAPACITY 64
#define MAX_FD_COUNT 4096

/**
 * Mapping entry from file descriptor to context and event mask
 */
typedef struct fd_mapping_s {
    socket_fd_t fd;
    void* context;
    int events;
} fd_mapping_t;

struct async_runtime_s {
    struct pollfd* pollfds;
    fd_mapping_t* mappings;
    int capacity;
    int count;
    
    /* Notification pipe for worker completions */
    int notify_pipe[2];
    
    console_type_t console_type;  /* Detected console type */
};

/* Helper functions */

static int find_fd_index(async_runtime_t* runtime, socket_fd_t fd) {
    for (int i = 0; i < runtime->count; i++) {
        if (runtime->pollfds[i].fd == fd) {
            return i;
        }
    }
    return -1;
}

static short events_to_poll(int events) {
    short poll_events = 0;
    if (events & EVENT_READ) poll_events |= POLLIN;
    if (events & EVENT_WRITE) poll_events |= POLLOUT;
    return poll_events;
}

static int poll_to_events(short revents) {
    int events = 0;
    if (revents & POLLIN) events |= EVENT_READ;
    if (revents & POLLOUT) events |= EVENT_WRITE;
    if (revents & POLLERR) events |= EVENT_ERROR;
    if (revents & (POLLHUP | POLLNVAL)) events |= EVENT_CLOSE;
    return events;
}

static int expand_capacity(async_runtime_t* runtime) {
    int new_capacity = runtime->capacity * 2;
    if (new_capacity > MAX_FD_COUNT) new_capacity = MAX_FD_COUNT;
    
    struct pollfd* new_pollfds = realloc(runtime->pollfds,
                                         new_capacity * sizeof(struct pollfd));
    if (!new_pollfds) return -1;
    runtime->pollfds = new_pollfds;
    
    fd_mapping_t* new_mappings = realloc(runtime->mappings,
                                         new_capacity * sizeof(fd_mapping_t));
    if (!new_mappings) return -1;
    runtime->mappings = new_mappings;
    
    for (int i = runtime->capacity; i < new_capacity; i++) {
        runtime->pollfds[i].fd = -1;
        runtime->pollfds[i].events = 0;
        runtime->pollfds[i].revents = 0;
        runtime->mappings[i].fd = -1;
        runtime->mappings[i].context = NULL;
        runtime->mappings[i].events = 0;
    }
    
    runtime->capacity = new_capacity;
    return 0;
}

/* Public API */

async_runtime_t* async_runtime_init(void) {
    async_runtime_t* runtime = calloc(1, sizeof(async_runtime_t));
    if (!runtime) return NULL;
    
    runtime->pollfds = malloc(INITIAL_CAPACITY * sizeof(struct pollfd));
    runtime->mappings = malloc(INITIAL_CAPACITY * sizeof(fd_mapping_t));
    
    if (!runtime->pollfds || !runtime->mappings) {
        free(runtime->pollfds);
        free(runtime->mappings);
        free(runtime);
        return NULL;
    }
    
    runtime->capacity = INITIAL_CAPACITY;
    runtime->count = 0;
    
    for (int i = 0; i < INITIAL_CAPACITY; i++) {
        runtime->pollfds[i].fd = -1;
        runtime->pollfds[i].events = 0;
        runtime->pollfds[i].revents = 0;
        runtime->mappings[i].fd = -1;
        runtime->mappings[i].context = NULL;
        runtime->mappings[i].events = 0;
    }
    
    /* Create notification pipe */
    if (pipe(runtime->notify_pipe) < 0) {
        free(runtime->pollfds);
        free(runtime->mappings);
        free(runtime);
        return NULL;
    }
    
    /* Make read end non-blocking */
    int flags = fcntl(runtime->notify_pipe[0], F_GETFL, 0);
    fcntl(runtime->notify_pipe[0], F_SETFL, flags | O_NONBLOCK);
    
    /* Add notify pipe to poll set */
    runtime->pollfds[0].fd = runtime->notify_pipe[0];
    runtime->pollfds[0].events = POLLIN;
    runtime->mappings[0].fd = runtime->notify_pipe[0];
    runtime->mappings[0].context = NULL;
    runtime->mappings[0].events = EVENT_READ;
    runtime->count = 1;
    
    return runtime;
}

void async_runtime_deinit(async_runtime_t* runtime) {
    if (!runtime) return;
    
    if (runtime->notify_pipe[0] >= 0) close(runtime->notify_pipe[0]);
    if (runtime->notify_pipe[1] >= 0) close(runtime->notify_pipe[1]);
    
    free(runtime->pollfds);
    free(runtime->mappings);
    free(runtime);
}

int async_runtime_add(async_runtime_t* runtime, socket_fd_t fd, uint32_t events, void* context) {
    if (!runtime || fd < 0) return -1;
    
    if (find_fd_index(runtime, fd) >= 0) return -1;  /* Already registered */
    
    if (runtime->count >= runtime->capacity) {
        if (expand_capacity(runtime) < 0) return -1;
    }
    
    int idx = runtime->count++;
    runtime->pollfds[idx].fd = fd;
    runtime->pollfds[idx].events = events_to_poll(events);
    runtime->pollfds[idx].revents = 0;
    runtime->mappings[idx].fd = fd;
    runtime->mappings[idx].context = context;
    runtime->mappings[idx].events = events;
    
    return 0;
}

int async_runtime_modify(async_runtime_t* runtime, socket_fd_t fd, uint32_t events, void* context) {
    if (!runtime || fd < 0) return -1;
    
    int idx = find_fd_index(runtime, fd);
    if (idx < 0) return -1;
    
    runtime->pollfds[idx].events = events_to_poll(events);
    runtime->mappings[idx].events = events;
    runtime->mappings[idx].context = context;  /* Update context when modifying events */
    
    return 0;
}

int async_runtime_remove(async_runtime_t* runtime, socket_fd_t fd) {
    if (!runtime || fd < 0) return -1;
    
    int idx = find_fd_index(runtime, fd);
    if (idx < 0) return -1;
    
    /* Move last entry to this slot */
    if (idx < runtime->count - 1) {
        runtime->pollfds[idx] = runtime->pollfds[runtime->count - 1];
        runtime->mappings[idx] = runtime->mappings[runtime->count - 1];
    }
    
    runtime->count--;
    runtime->pollfds[runtime->count].fd = -1;
    runtime->mappings[runtime->count].fd = -1;
    
    return 0;
}

int async_runtime_wakeup(async_runtime_t* runtime) {
    if (!runtime || runtime->notify_pipe[1] < 0) return -1;
    
    char byte = 1;
    ssize_t n = write(runtime->notify_pipe[1], &byte, 1);
    return (n == 1) ? 0 : -1;
}

int async_runtime_wait(async_runtime_t* runtime, io_event_t* events,
                       int max_events, struct timeval* timeout) {
    if (!runtime || !events || max_events <= 0) return -1;
    
    int timeout_ms = -1;
    if (timeout) {
        timeout_ms = (timeout->tv_sec * 1000) + (timeout->tv_usec / 1000);
    }
    
    int result = poll(runtime->pollfds, runtime->count, timeout_ms);
    if (result < 0) {
        /* EINTR (signal interruption) is used to wake up the event loop.
         * Treat it as timeout so backend can check heartbeat/shutdown flags. */
        return (errno == EINTR) ? 0 : -1;
    }
    if (result == 0) return 0;  /* Timeout */
    
    int event_count = 0;
    for (int i = 0; i < runtime->count && event_count < max_events; i++) {
        if (runtime->pollfds[i].revents) {
            /* Check if this is the notify pipe */
            if (runtime->pollfds[i].fd == runtime->notify_pipe[0]) {
                /* Drain the pipe */
                uint64_t val;
                while (read(runtime->notify_pipe[0], &val, sizeof(val)) == sizeof(val)) {
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
                events[event_count].fd = runtime->pollfds[i].fd;
                events[event_count].completion_key = 0;
                events[event_count].context = runtime->mappings[i].context;
                events[event_count].event_type = poll_to_events(runtime->pollfds[i].revents);
                events[event_count].bytes_transferred = 0;
                events[event_count].buffer = NULL;
                event_count++;
            }
        }
    }
    
    return event_count;
}

int async_runtime_post_completion(async_runtime_t* runtime, uintptr_t completion_key, uintptr_t data) {
    if (!runtime || runtime->notify_pipe[1] < 0) return -1;
    
    uint64_t val = (((uint64_t)completion_key) << 32) | (data & 0xFFFFFFFF);
    ssize_t n = write(runtime->notify_pipe[1], &val, sizeof(val));
    
    return (n == sizeof(val)) ? 0 : -1;
}

int async_runtime_post_read(async_runtime_t* runtime, socket_fd_t fd, void* buffer, size_t len) {
    /* No-op on poll (readiness-based) */
    (void)runtime; (void)fd; (void)buffer; (void)len;
    return 0;
}

int async_runtime_post_write(async_runtime_t* runtime, socket_fd_t fd, void* buffer, size_t len) {
    /* No-op on poll (readiness-based) */
    (void)runtime; (void)fd; (void)buffer; (void)len;
    return 0;
}

int async_runtime_get_event_loop_handle(async_runtime_t* runtime) {
    return runtime ? runtime->notify_pipe[0] : -1;
}

int async_runtime_add_console(async_runtime_t* runtime, void* context) {
    if (!runtime) return -1;
    
    (void)context;  /* Console context not used on POSIX */
    
    /* Detect console type using isatty() and fstat() */
    if (isatty(STDIN_FILENO)) {
        runtime->console_type = CONSOLE_TYPE_REAL;
    } else {
        struct stat st;
        if (fstat(STDIN_FILENO, &st) == 0) {
            if (S_ISFIFO(st.st_mode)) {
                runtime->console_type = CONSOLE_TYPE_PIPE;
            } else if (S_ISREG(st.st_mode)) {
                runtime->console_type = CONSOLE_TYPE_FILE;
            } else {
                runtime->console_type = CONSOLE_TYPE_NONE;
            }
        } else {
            runtime->console_type = CONSOLE_TYPE_NONE;
        }
    }
    
    return 0;
}

console_type_t async_runtime_get_console_type(async_runtime_t* runtime) {
    return runtime ? runtime->console_type : CONSOLE_TYPE_NONE;
}

#endif /* !_WIN32 && !__linux__ */
