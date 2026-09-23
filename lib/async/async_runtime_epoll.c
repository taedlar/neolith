/**
 * @file async_runtime_epoll.c
 * @brief Linux epoll-based async runtime implementation
 * 
 * Uses epoll for efficient I/O multiplexing and eventfd for worker completion notifications.
 */

#if defined(__linux__)
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#define NO_STEM
#include "src/std.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/stat.h>
#include <errno.h>

#include "async/async_runtime.h"

#define MAX_EVENTS 64

typedef struct async_registration_s {
    socket_fd_t fd;
    void* context;
    struct async_registration_s* next;
} async_registration_t;

struct async_runtime_s {
    int epoll_fd;
    int event_fd;  /* For worker completions */
    console_type_t console_type;  /* Detected console type */
    async_registration_t event_registration;
    async_registration_t* registrations;
    async_registration_t* retired_registrations;
};

/* Helper functions */

static async_registration_t* find_registration(async_runtime_t* runtime, socket_fd_t fd) {
    async_registration_t* registration;

    for (registration = runtime->registrations; registration; registration = registration->next) {
        if (registration->fd == fd) return registration;
    }

    return NULL;
}

static async_registration_t* create_registration(socket_fd_t fd, void* context) {
    async_registration_t* registration = malloc(sizeof(*registration));

    if (!registration) return NULL;

    registration->fd = fd;
    registration->context = context;
    registration->next = NULL;
    return registration;
}

static void free_registrations(async_registration_t* registrations) {
    while (registrations) {
        async_registration_t* next = registrations->next;
        free(registrations);
        registrations = next;
    }
}

static void reclaim_retired_registrations(async_runtime_t* runtime) {
    free_registrations(runtime->retired_registrations);
    runtime->retired_registrations = NULL;
}

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
    runtime->event_registration.fd = runtime->event_fd;
    ev.data.ptr = &runtime->event_registration;
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

    free_registrations(runtime->registrations);
    free_registrations(runtime->retired_registrations);
    
    free(runtime);
}

int async_runtime_add(async_runtime_t* runtime, socket_fd_t fd, uint32_t events, void* context) {
    async_registration_t* registration;

    if (!runtime || fd < 0) return -1;
    if (find_registration(runtime, fd)) return -1;

    registration = create_registration(fd, context);
    if (!registration) return -1;
    
    struct epoll_event ev = {0};
    ev.events = events_to_epoll(events);
    ev.data.ptr = registration;
    
    if (epoll_ctl(runtime->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        free(registration);
        return -1;
    }

    registration->next = runtime->registrations;
    runtime->registrations = registration;

    return 0;
}

int async_runtime_modify(async_runtime_t* runtime, socket_fd_t fd, uint32_t events, void* context) {
    async_registration_t* registration;

    if (!runtime || fd < 0) return -1;
    registration = find_registration(runtime, fd);
    if (!registration) return -1;
    
    struct epoll_event ev = {0};
    ev.events = events_to_epoll(events);
    ev.data.ptr = registration;
    
    if (epoll_ctl(runtime->epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) return -1;
    registration->context = context;
    return 0;
}

int async_runtime_remove(async_runtime_t* runtime, socket_fd_t fd) {
    async_registration_t* registration;
    async_registration_t** link;

    if (!runtime || fd < 0) return -1;
    registration = find_registration(runtime, fd);
    if (!registration) return -1;
    
    if (epoll_ctl(runtime->epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) return -1;

    for (link = &runtime->registrations; *link != registration; link = &(*link)->next) {
    }
    *link = registration->next;
    registration->next = runtime->retired_registrations;
    runtime->retired_registrations = registration;

    return 0;
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

    reclaim_retired_registrations(runtime);
    
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
        async_registration_t* registration = epoll_events[i].data.ptr;

        /* Check if this is the eventfd */
        if (registration == &runtime->event_registration) {
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
            events[event_count].fd = registration->fd;
            events[event_count].completion_key = 0;
            events[event_count].context = registration->context;
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

#endif /* __linux__ */
