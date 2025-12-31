#pragma once

/**
 * @file io_reactor.h
 * @brief Platform-agnostic I/O reactor abstraction for event-driven I/O multiplexing.
 *
 * This header defines the Reactor Pattern abstraction for cross-platform non-blocking
 * I/O in Neolith. The reactor decouples event detection (poll, epoll, IOCP) from
 * event handling logic in the backend loop.
 *
 * Platform-specific implementations:
 * - POSIX: io_reactor_poll.c (using poll() or epoll())
 * - Windows: io_reactor_win32.c (using I/O Completion Ports)
 *
 * Design: docs/manual/io-reactor.md
 */

#include "socket_comm.h"
#ifndef WINSOCK
#include <sys/time.h>
#endif

/* Event type flags */
#define EVENT_READ   0x01  /* Socket/fd is readable */
#define EVENT_WRITE  0x02  /* Socket/fd is writable */
#define EVENT_ERROR  0x04  /* Error occurred on socket/fd */
#define EVENT_CLOSE  0x08  /* Connection closed (EOF or remote shutdown) */

/**
 * @brief Event structure returned by io_reactor_wait().
 *
 * Each event represents a state change on a registered file descriptor.
 * The reactor fills in these fields when events occur.
 */
typedef struct io_event_s {
    void *context;              /**< User-supplied context pointer (e.g., interactive_t*) */
    int event_type;             /**< Bitmask of EVENT_* flags */
    int bytes_transferred;      /**< Bytes transferred (for completion-based I/O, e.g., IOCP) */
    void *buffer;               /**< Buffer associated with I/O operation (platform-specific) */
} io_event_t;

/**
 * @brief Opaque reactor handle.
 *
 * The internal structure is platform-specific and defined in implementation files.
 */
typedef struct io_reactor_s io_reactor_t;

/*
 * =============================================================================
 * Lifecycle Management
 * =============================================================================
 */

/**
 * @brief Create a new I/O reactor instance.
 *
 * Allocates and initializes platform-specific resources for event demultiplexing.
 * On Linux, this may create an epoll instance. On Windows, this creates an
 * I/O Completion Port.
 *
 * @return Pointer to newly created reactor, or NULL on failure.
 *
 * Example:
 * @code
 *   io_reactor_t *reactor = io_reactor_create();
 *   if (!reactor) {
 *       debug_fatal("Failed to create I/O reactor\n");
 *   }
 * @endcode
 */
io_reactor_t* io_reactor_create(void);

/**
 * @brief Destroy an I/O reactor and release all resources.
 *
 * Closes all registered file descriptors (platform-dependent), frees internal
 * data structures, and invalidates the reactor handle.
 *
 * @param reactor The reactor to destroy. Must not be NULL.
 *
 * @note After calling this function, the reactor pointer is invalid and must
 *       not be used.
 */
void io_reactor_destroy(io_reactor_t *reactor);

/*
 * =============================================================================
 * Handle Registration
 * =============================================================================
 */

/**
 * @brief Register a file descriptor/socket with the reactor.
 *
 * Adds the file descriptor to the reactor's monitoring set. The reactor will
 * watch for the specified events and return them via io_reactor_wait().
 *
 * @param reactor The reactor instance. Must not be NULL.
 * @param fd The file descriptor or socket to monitor. Must be valid.
 * @param context User-supplied context pointer. Stored by the reactor and
 *                returned in io_event_t when events occur. Typically points
 *                to interactive_t, port_def_t, or lpc_socket_t. May be NULL.
 * @param events Bitmask of EVENT_READ and/or EVENT_WRITE to monitor.
 * @return 0 on success, -1 on failure.
 *
 * @note The file descriptor must be in non-blocking mode for proper operation.
 * @note On some platforms (Windows IOCP), this may post initial async operations.
 *
 * Example:
 * @code
 *   if (io_reactor_add(reactor, socket_fd, user_context, EVENT_READ) != 0) {
 *       debug_message("Failed to register socket %d\n", socket_fd);
 *   }
 * @endcode
 */
int io_reactor_add(io_reactor_t *reactor, socket_fd_t fd, void *context, int events);

/**
 * @brief Modify the event mask for a registered file descriptor.
 *
 * Changes the set of events being monitored for an already-registered file
 * descriptor. Use this when transitioning between read-only, write-only, or
 * bidirectional monitoring.
 *
 * @param reactor The reactor instance. Must not be NULL.
 * @param fd The file descriptor to modify. Must be already registered.
 * @param events New event mask (EVENT_READ | EVENT_WRITE). Use 0 to stop
 *               monitoring events but keep the descriptor registered.
 * @return 0 on success, -1 on failure.
 *
 * Example:
 * @code
 *   // Switch from read-only to bidirectional
 *   io_reactor_modify(reactor, fd, EVENT_READ | EVENT_WRITE);
 * @endcode
 */
int io_reactor_modify(io_reactor_t *reactor, socket_fd_t fd, int events);

/**
 * @brief Unregister a file descriptor from the reactor.
 *
 * Removes the file descriptor from the reactor's monitoring set. No further
 * events will be delivered for this descriptor. The reactor does NOT close
 * the file descriptor; the caller retains ownership.
 *
 * @param reactor The reactor instance. Must not be NULL.
 * @param fd The file descriptor to remove. Must be already registered.
 * @return 0 on success, -1 on failure.
 *
 * @note It is safe to call this function even if the descriptor was not
 *       registered (implementation should handle gracefully).
 *
 * Example:
 * @code
 *   io_reactor_remove(reactor, socket_fd);
 *   SOCKET_CLOSE(socket_fd);
 * @endcode
 */
int io_reactor_remove(io_reactor_t *reactor, socket_fd_t fd);

/*
 * =============================================================================
 * Event Loop Integration
 * =============================================================================
 */

/**
 * @brief Wait for I/O events and return them.
 *
 * This is the core event demultiplexing function. It blocks until one or more
 * I/O events occur, the timeout expires, or an error happens.
 *
 * On readiness-based platforms (Linux poll/epoll), this returns when file
 * descriptors become ready for I/O. The application must then perform the
 * actual read/write operations.
 *
 * On completion-based platforms (Windows IOCP), this returns when previously
 * posted async operations complete, with data already in buffers.
 *
 * @param reactor The reactor instance. Must not be NULL.
 * @param events Array to store returned events. Must not be NULL.
 * @param max_events Maximum number of events to return. Must be > 0.
 * @param timeout Pointer to timeout structure, or NULL to block indefinitely.
 *                A zero timeout (tv_sec=0, tv_usec=0) results in non-blocking
 *                poll behavior.
 * @return Number of events returned (>= 0), or -1 on error.
 *         Returns 0 if timeout expired with no events.
 *
 * @note The reactor may return fewer events than max_events even if more are
 *       available. Call again to retrieve additional events.
 * @note After timeout expiration, the timeout structure contents are undefined.
 *
 * Example:
 * @code
 *   io_event_t events[128];
 *   struct timeval timeout = {1, 0};  // 1 second
 *   int n = io_reactor_wait(reactor, events, 128, &timeout);
 *   if (n < 0) {
 *       debug_perror("io_reactor_wait failed");
 *   } else if (n == 0) {
 *       // Timeout, no events
 *   } else {
 *       for (int i = 0; i < n; i++) {
 *           process_event(&events[i]);
 *       }
 *   }
 * @endcode
 */
int io_reactor_wait(io_reactor_t *reactor, io_event_t *events,
                    int max_events, struct timeval *timeout);

/*
 * =============================================================================
 * Platform-Specific Helpers
 * =============================================================================
 *
 * These functions abstract differences in I/O models:
 * - On POSIX systems with readiness notification (poll/epoll), these may be
 *   no-ops, as I/O occurs after EVENT_READ/EVENT_WRITE notification.
 * - On Windows with completion notification (IOCP), these post async operations
 *   that complete later via io_reactor_wait().
 */

/**
 * @brief Post an asynchronous read operation (platform-specific).
 *
 * On POSIX systems: Typically a no-op. The application reads data after
 * receiving EVENT_READ from io_reactor_wait().
 *
 * On Windows IOCP: Posts an async WSARecv() operation. The read completes
 * asynchronously, and io_reactor_wait() returns an EVENT_READ with data
 * already in the buffer.
 *
 * @param reactor The reactor instance. Must not be NULL.
 * @param fd The file descriptor to read from. Must be registered.
 * @param buffer Buffer to read into. If NULL, the reactor may use an internal
 *               buffer (platform-specific).
 * @param len Size of buffer in bytes.
 * @return 0 on success, -1 on failure.
 *
 * @note This function is primarily for Windows IOCP compatibility. POSIX
 *       implementations may ignore it or use it for future optimizations.
 */
int io_reactor_post_read(io_reactor_t *reactor, socket_fd_t fd, void *buffer, size_t len);

/**
 * @brief Post an asynchronous write operation (platform-specific).
 *
 * On POSIX systems: Typically a no-op. The application writes data after
 * receiving EVENT_WRITE from io_reactor_wait().
 *
 * On Windows IOCP: Posts an async WSASend() operation. The write completes
 * asynchronously, and io_reactor_wait() returns an EVENT_WRITE when done.
 *
 * @param reactor The reactor instance. Must not be NULL.
 * @param fd The file descriptor to write to. Must be registered.
 * @param buffer Buffer containing data to write. Must not be NULL.
 * @param len Number of bytes to write from the buffer.
 * @return 0 on success, -1 on failure.
 *
 * @note This function is primarily for Windows IOCP compatibility. POSIX
 *       implementations may ignore it or use it for future optimizations.
 */
int io_reactor_post_write(io_reactor_t *reactor, socket_fd_t fd, void *buffer, size_t len);
/*
 * =============================================================================
 * Console Support (Windows-specific)
 * =============================================================================
 */

#ifdef _WIN32
/**
 * @brief Register Windows console input for event monitoring.
 *
 * Windows console I/O is not socket-based and cannot use standard IOCP or
 * Winsock select(). This function enables polling of console input events
 * in the reactor's event loop.
 *
 * @param reactor The reactor instance. Must not be NULL.
 * @param context User context pointer (returned in console events).
 * @return 0 on success, -1 on failure (e.g., not a console, redirected I/O).
 *
 * Example:
 * @code
 *   #define CONSOLE_CONTEXT_MARKER ((void*)0x1)
 *   if (io_reactor_add_console(reactor, CONSOLE_CONTEXT_MARKER) != 0) {
 *       debug_message("Warning: Failed to register console input\n");
 *   }
 * @endcode
 */
int io_reactor_add_console(io_reactor_t *reactor, void *context);
#endif