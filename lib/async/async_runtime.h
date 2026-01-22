/**
 * @file async_runtime.h
 * @brief Unified event loop runtime for I/O events and worker completions
 *
 * The async runtime provides event-driven I/O multiplexing combined with
 * worker thread completion notifications in a single unified event loop.
 *
 * Platform-specific implementations:
 * - Windows: async_runtime_iocp.c (using I/O Completion Ports)
 *   - Uses dedicated accept worker thread for listening sockets
 *   - Worker calls accept() and posts completed FD to IOCP
 * - Linux: async_runtime_epoll.c (using epoll + eventfd for completions)
 *   - Traditional readiness notification for listening sockets
 * - Fallback: async_runtime_poll.c (using poll + pipe for completions)
 *   - Traditional readiness notification for listening sockets
 *
 * Design: docs/internals/async-library.md
 */

#ifndef ASYNC_RUNTIME_H
#define ASYNC_RUNTIME_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
typedef SOCKET socket_fd_t;
#else
typedef int socket_fd_t;
#include <sys/time.h>
#endif

/* Event type flags */
#define EVENT_READ   0x01  /**< Socket/fd is readable */
#define EVENT_WRITE  0x02  /**< Socket/fd is writable */
#define EVENT_ERROR  0x04  /**< Error occurred on socket/fd */
#define EVENT_CLOSE  0x08  /**< Connection closed (EOF or remote shutdown) */

/**
 * Event structure returned by async_runtime_wait()
 * 
 * Represents either an I/O event (fd/handle became ready) or a worker
 * completion (worker posted completion via async_runtime_post_completion).
 * 
 * On Windows with listening sockets:
 * - fd contains the ACCEPTED socket FD (not the listening socket)
 * - context points to the listening port_def_t structure
 * - Accept worker has already called accept() before posting completion
 */
typedef struct {
    socket_fd_t fd;              /**< File descriptor; on Windows may be accepted socket from accept worker */
#ifdef _WIN32
    HANDLE handle;               /**< Native handle (Windows) or NULL */
#endif
    uintptr_t completion_key;    /**< User-defined key for completion correlation */
    uint32_t event_type;         /**< Bitmask of EVENT_* flags */
    void* context;               /**< User context pointer */
    size_t bytes_transferred;       /**< Bytes transferred (completion-based I/O) */
    void* buffer;                /**< Buffer associated with I/O operation */
} io_event_t;

/**
 * Console input types (Windows-specific)
 */
#ifdef _WIN32
typedef enum {
    CONSOLE_TYPE_NONE = 0,
    CONSOLE_TYPE_REAL,    /**< Real console (ReadConsoleInputW) */
    CONSOLE_TYPE_PIPE,    /**< Pipe (synchronous ReadFile) */
    CONSOLE_TYPE_FILE     /**< File (synchronous ReadFile) */
} console_type_t;
#endif

/**
 * Opaque runtime handle
 */
typedef struct async_runtime_s async_runtime_t;

/*
 * =============================================================================
 * Lifecycle Management
 * =============================================================================
 */

/**
 * Initialize the async runtime
 * 
 * Creates platform-specific resources for event demultiplexing and worker
 * completion notifications.
 * 
 * @returns Runtime handle, or NULL on failure
 */
async_runtime_t* async_runtime_init(void);

/**
 * Destroy the async runtime and release all resources
 * 
 * @param runtime Runtime to destroy
 */
void async_runtime_deinit(async_runtime_t* runtime);

/*
 * =============================================================================
 * I/O Source Management
 * =============================================================================
 */

/**
 * Register a file descriptor/socket with the runtime
 * 
 * For listening sockets on Windows, the accept worker will monitor the socket
 * and post accepted FD completions to IOCP with the context pointer.
 * 
 * For connected sockets, the context is returned in io_event_t when I/O events occur.
 * 
 * @param runtime Runtime instance
 * @param fd File descriptor or socket to monitor
 * @param events Bitmask of EVENT_READ and/or EVENT_WRITE
 * @param context User-supplied context pointer (returned in io_event_t.context)
 * @returns 0 on success, -1 on failure
 */
int async_runtime_add(async_runtime_t* runtime, socket_fd_t fd, uint32_t events, void* context);

/**
 * Modify event mask for registered descriptor
 * 
 * Used to toggle between EVENT_READ and EVENT_READ|EVENT_WRITE for flow control.
 * 
 * @param runtime Runtime instance
 * @param fd File descriptor to modify
 * @param events New event mask (EVENT_READ and/or EVENT_WRITE)
 * @param context User-supplied context pointer (should match original, used for validation)
 * @returns 0 on success, -1 on failure
 */
int async_runtime_modify(async_runtime_t* runtime, socket_fd_t fd, uint32_t events, void* context);

/**
 * Unregister a file descriptor from the runtime
 * 
 * @param runtime Runtime instance
 * @param fd File descriptor to remove
 * @returns 0 on success, -1 on failure
 */
int async_runtime_remove(async_runtime_t* runtime, socket_fd_t fd);

/**
 * Wake up a blocked async_runtime_wait() call
 * 
 * Thread-safe, can be called from signal handlers or timer callbacks.
 * 
 * @param runtime Runtime instance
 * @returns 0 on success, -1 on failure
 */
int async_runtime_wakeup(async_runtime_t* runtime);

/*
 * =============================================================================
 * Event Loop (Unified I/O + Worker Completions)
 * =============================================================================
 */

/**
 * Wait for I/O events or worker completions
 * 
 * Blocks until one of the following occurs:
 * - I/O events occur (connected sockets become ready for read/write)
 * - Listening socket accepts connection (Windows: accept worker posts accepted FD;
 *   POSIX: listening socket becomes readable, caller must call accept())
 * - Worker completion posted via async_runtime_post_completion()
 * - Timeout expires
 * 
 * On Windows, listening socket events have:
 * - io_event_t.fd = accepted socket (not listening socket)
 * - io_event_t.context = listening socket's context (e.g., port_def_t*)
 * - Caller should use getpeername() to get peer address
 * 
 * On POSIX, listening socket events have:
 * - io_event_t.context = listening socket's context
 * - Caller must call accept() to get new connection
 * 
 * CRITICAL: This function MUST be called from a single thread only (the main thread).
 * Double polling (calling from multiple threads or calling again while a previous
 * call is still blocked) will cause undefined behavior:
 * - Windows IOCP: Events may be delivered to wrong thread or lost
 * - Linux epoll: Spurious wakeups and event duplication
 * - Event correlation breaks (completion_key may mismatch context)
 * 
 * The driver backend calls this via do_comm_polling() in the main event loop.
 * Never call this function from worker threads or multiple locations.
 * 
 * @param runtime Runtime instance
 * @param events Array to store returned events
 * @param max_events Maximum number of events to return
 * @param timeout Pointer to timeout structure (NULL = block indefinitely)
 * @returns Number of events returned (>= 0), or -1 on error
 */
int async_runtime_wait(async_runtime_t* runtime, io_event_t* events,
                       int max_events, struct timeval* timeout);

/**
 * Post a worker completion (called from worker threads)
 * 
 * Wakes up async_runtime_wait() and delivers a completion event with
 * the specified completion_key. Main thread uses completion_key to identify
 * the completion source (e.g., CONSOLE_COMPLETION_KEY for console worker).
 * 
 * Thread-safe, can be called from any worker thread.
 * 
 * Platform implementation:
 * - Windows: PostQueuedCompletionStatus() to IOCP
 * - Linux: write() to eventfd
 * - macOS/BSD: write() to pipe
 * 
 * Usage example:
 * - Console worker: completion_key = CONSOLE_COMPLETION_KEY, data = unused
 * - Accept worker (internal): Uses ACCEPT_COMPLETION_KEY, data = accepted FD
 * 
 * @param runtime Runtime instance
 * @param completion_key User-defined key for identifying completion source
 * @param data Optional data value (platform-specific, often unused)
 * @returns 0 on success, -1 on failure
 */
int async_runtime_post_completion(async_runtime_t* runtime, uintptr_t completion_key, uintptr_t data);

/*
 * =============================================================================
 * Platform-Specific Helpers
 * =============================================================================
 */

/**
 * Post asynchronous read operation (Windows IOCP only)
 * 
 * @param runtime Runtime instance
 * @param fd File descriptor to read from
 * @param buffer Buffer to read into
 * @param len Buffer size in bytes
 * @returns 0 on success, -1 on failure
 */
int async_runtime_post_read(async_runtime_t* runtime, socket_fd_t fd, void* buffer, size_t len);

/**
 * Post asynchronous write operation (Windows IOCP only)
 * 
 * @param runtime Runtime instance
 * @param fd File descriptor to write to
 * @param buffer Buffer containing data to write
 * @param len Number of bytes to write
 * @returns 0 on success, -1 on failure
 */
int async_runtime_post_write(async_runtime_t* runtime, socket_fd_t fd, void* buffer, size_t len);

/*
 * =============================================================================
 * Console Support (Windows-specific)
 * =============================================================================
 */

#ifdef _WIN32
/**
 * Register Windows console input for event monitoring
 * 
 * @param runtime Runtime instance
 * @param context User context pointer
 * @returns 0 on success, -1 on failure
 */
int async_runtime_add_console(async_runtime_t* runtime, void* context);

/**
 * Get console type
 * 
 * @param runtime Runtime instance
 * @returns Console type
 */
console_type_t async_runtime_get_console_type(async_runtime_t* runtime);

/**
 * Get console event handle for piped stdin
 * 
 * @param runtime Runtime instance
 * @returns Event handle or NULL
 */
HANDLE async_runtime_get_console_event(async_runtime_t* runtime);

/**
 * Get console IOCP context for piped stdin
 * 
 * @param runtime Runtime instance
 * @returns IOCP context pointer or NULL
 */
void* async_runtime_get_console_ctx(async_runtime_t* runtime);

/**
 * Expose IOCP handle for worker integration
 * 
 * Allows workers to call PostQueuedCompletionStatus() directly.
 * 
 * @param runtime Runtime instance
 * @returns IOCP handle
 */
HANDLE async_runtime_get_iocp(async_runtime_t* runtime);
#else
/**
 * Get eventfd/pipe handle for worker notification (POSIX)
 * 
 * Returns file descriptor used for completion notification.
 * 
 * @param runtime Runtime instance
 * @returns Event loop notification handle
 */
int async_runtime_get_event_loop_handle(async_runtime_t* runtime);
#endif

#endif /* ASYNC_RUNTIME_H */
