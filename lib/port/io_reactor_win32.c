/**
 * @file io_reactor_win32.c
 * @brief Windows I/O Completion Ports (IOCP) based I/O reactor implementation.
 *
 * This implementation uses Windows I/O Completion Ports for event demultiplexing.
 * IOCP provides completion notification: async operations are posted, and the
 * reactor returns when they complete with data already in buffers.
 *
 * Platform: Windows XP and later
 * Performance: O(1) completion retrieval
 * Scalability: Excellent for 1000+ connections
 *
 * Design: docs/manual/windows-io.md
 */

#include "io_reactor.h"
#include "socket_comm.h"
#include <windows.h>
#include <winsock2.h>
#include <stdlib.h>
#include <string.h>

/* Maximum text buffer size from existing code (MAX_TEXT typically 2048) */
#ifndef MAX_TEXT
#define MAX_TEXT 2048
#endif

/* Initial capacity for context pool */
#define INITIAL_POOL_SIZE 256

/* Operation types for IOCP contexts */
#define OP_READ    1
#define OP_WRITE   2
#define OP_ACCEPT  3

/**
 * @brief IOCP context for each I/O operation.
 *
 * This structure represents a single overlapped I/O operation.
 * The OVERLAPPED structure must be the first member to support
 * CONTAINING_RECORD macro for retrieving the full context from
 * the OVERLAPPED pointer returned by GetQueuedCompletionStatus().
 */
typedef struct iocp_context_s {
    OVERLAPPED overlapped;       /* Must be first member */
    void *user_context;          /* User-supplied context (interactive_t*, etc.) */
    int operation;               /* OP_READ, OP_WRITE, OP_ACCEPT */
    WSABUF wsa_buf;              /* WSA buffer descriptor */
    char buffer[MAX_TEXT];       /* Inline buffer to avoid allocations */
    socket_fd_t fd;              /* Associated file descriptor */
} iocp_context_t;

/**
 * @brief Listening socket entry for tracking sockets that need select() polling.
 */
typedef struct listening_socket_s {
    socket_fd_t fd;
    void *context;
} listening_socket_t;

/**
 * @brief Windows IOCP reactor implementation.
 */
struct io_reactor_s {
    HANDLE iocp_handle;          /* I/O completion port handle */
    int num_fds;                 /* Number of registered handles */
    
    /* Context pool for allocation efficiency */
    iocp_context_t **context_pool;
    int pool_size;               /* Current number of contexts in pool */
    int pool_capacity;           /* Maximum pool capacity */
    
    /* Listening sockets tracked separately (need select() instead of IOCP) */
    listening_socket_t *listen_sockets;
    int listen_count;            /* Number of listening sockets */
    int listen_capacity;         /* Capacity of listen_sockets array */
    
    /* Console support (Windows doesn't support STDIN in Winsock select) */
    HANDLE console_handle;       /* Console input handle (STD_INPUT_HANDLE) */
    void *console_context;       /* User context for console events */
    int console_enabled;         /* Whether console monitoring is active */
};

/*
 * =============================================================================
 * Context Pool Management
 * =============================================================================
 */

/**
 * @brief Allocate an IOCP context from the pool or create a new one.
 * @param reactor The reactor instance.
 * @param fd File descriptor for this operation.
 * @param user_context User context pointer.
 * @param operation Operation type (OP_READ, OP_WRITE, etc.).
 * @return Allocated context, or NULL on failure.
 */
static iocp_context_t* alloc_iocp_context(io_reactor_t *reactor, socket_fd_t fd,
                                          void *user_context, int operation) {
    iocp_context_t *ctx;
    
    /* Try to reuse from pool */
    if (reactor->pool_size > 0) {
        ctx = reactor->context_pool[--reactor->pool_size];
    } else {
        ctx = calloc(1, sizeof(iocp_context_t));
        if (!ctx) {
            return NULL;
        }
    }
    
    /* Initialize context */
    ZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));
    ctx->user_context = user_context;
    ctx->operation = operation;
    ctx->fd = fd;
    ctx->wsa_buf.buf = ctx->buffer;
    ctx->wsa_buf.len = sizeof(ctx->buffer);
    
    return ctx;
}

/**
 * @brief Return an IOCP context to the pool.
 * @param reactor The reactor instance.
 * @param ctx Context to free.
 */
static void free_iocp_context(io_reactor_t *reactor, iocp_context_t *ctx) {
    if (!ctx) {
        return;
    }
    
    /* Return to pool if not full */
    if (reactor->pool_size < reactor->pool_capacity) {
        reactor->context_pool[reactor->pool_size++] = ctx;
    } else {
        free(ctx);
    }
}

/*
 * =============================================================================
 * Lifecycle Management
 * =============================================================================
 */

io_reactor_t* io_reactor_create(void) {
    io_reactor_t *reactor = calloc(1, sizeof(io_reactor_t));
    if (!reactor) {
        return NULL;
    }
    
    /* Create IOCP with 1 concurrent thread (single-threaded model) */
    reactor->iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if (reactor->iocp_handle == NULL) {
        free(reactor);
        return NULL;
    }
    
    /* Initialize context pool */
    reactor->pool_capacity = INITIAL_POOL_SIZE;
    reactor->context_pool = calloc(reactor->pool_capacity, sizeof(iocp_context_t*));
    if (!reactor->context_pool) {
        CloseHandle(reactor->iocp_handle);
        free(reactor);
        return NULL;
    }
    
    /* Initialize listening socket tracking */
    reactor->listen_capacity = 16;  /* Reasonable initial size */
    reactor->listen_sockets = calloc(reactor->listen_capacity, sizeof(listening_socket_t));
    if (!reactor->listen_sockets) {
        free(reactor->context_pool);
        CloseHandle(reactor->iocp_handle);
        free(reactor);
        return NULL;
    }
    reactor->listen_count = 0;
    
    /* Initialize console support */
    reactor->console_handle = INVALID_HANDLE_VALUE;
    reactor->console_context = NULL;
    reactor->console_enabled = 0;
    
    reactor->pool_size = 0;
    reactor->num_fds = 0;
    
    return reactor;
}

void io_reactor_destroy(io_reactor_t *reactor) {
    if (!reactor) {
        return;
    }
    
    /* Free context pool */
    for (int i = 0; i < reactor->pool_size; i++) {
        free(reactor->context_pool[i]);
    }
    free(reactor->context_pool);
    
    /* Free listening socket tracking */
    free(reactor->listen_sockets);
    
    /* Close IOCP handle */
    if (reactor->iocp_handle != NULL && reactor->iocp_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(reactor->iocp_handle);
    }
    
    free(reactor);
}

/*
 * =============================================================================
 * Handle Registration
 * =============================================================================
 */

int io_reactor_add(io_reactor_t *reactor, socket_fd_t fd, void *context, int events) {
    if (!reactor || fd == INVALID_SOCKET) {
        return -1;
    }
    
    /* Check if this is a listening socket using SO_ACCEPTCONN */
    BOOL is_listening = FALSE;
    int optlen = sizeof(is_listening);
    if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, (char*)&is_listening, &optlen) == 0 && is_listening) {
        /* Listening socket - track separately for select() polling */
        if (reactor->listen_count >= reactor->listen_capacity) {
            /* Resize listening socket array */
            int new_capacity = reactor->listen_capacity * 2;
            listening_socket_t *new_array = realloc(reactor->listen_sockets,
                                                    new_capacity * sizeof(listening_socket_t));
            if (!new_array) {
                return -1;
            }
            reactor->listen_sockets = new_array;
            reactor->listen_capacity = new_capacity;
        }
        
        reactor->listen_sockets[reactor->listen_count].fd = fd;
        reactor->listen_sockets[reactor->listen_count].context = context;
        reactor->listen_count++;
        reactor->num_fds++;
        return 0;
    }
    
    /* Associate socket with IOCP
     * Note: The completion key is set to 0 here. We use the context stored
     * in iocp_context_t instead, which is more flexible for per-operation data.
     */
    HANDLE result = CreateIoCompletionPort((HANDLE)fd, reactor->iocp_handle, 0, 0);
    if (result == NULL) {
        return -1;
    }
    
    /* Post initial async read if requested
     * Note: We don't pass the context here because it will be set when
     * we post the actual read operation below.
     */
    if (events & EVENT_READ) {
        iocp_context_t *io_ctx = alloc_iocp_context(reactor, fd, context, OP_READ);
        if (!io_ctx) {
            return -1;
        }
        
        DWORD flags = 0;
        DWORD bytes_received = 0;
        
        int ret = WSARecv(fd, &io_ctx->wsa_buf, 1, &bytes_received,
                         &flags, &io_ctx->overlapped, NULL);
        
        if (ret == SOCKET_ERROR) {
            DWORD error = WSAGetLastError();
            if (error != WSA_IO_PENDING) {
                /* Immediate error */
                free_iocp_context(reactor, io_ctx);
                return -1;
            }
            /* WSA_IO_PENDING is expected for async operations */
        }
    }
    
    reactor->num_fds++;
    return 0;
}

int io_reactor_modify(io_reactor_t *reactor, socket_fd_t fd, int events) {
    /* With IOCP, event interest is managed by posting operations.
     * This is a no-op; callers should use io_reactor_post_read/write directly.
     * We return success to maintain API compatibility.
     */
    (void)reactor;
    (void)fd;
    (void)events;
    return 0;
}

int io_reactor_remove(io_reactor_t *reactor, socket_fd_t fd) {
    if (!reactor || fd == INVALID_SOCKET) {
        return -1;
    }
    
    /* Check if this is a listening socket and remove from tracking */
    for (int i = 0; i < reactor->listen_count; i++) {
        if (reactor->listen_sockets[i].fd == fd) {
            /* Remove by moving last element into this slot */
            reactor->listen_count--;
            if (i < reactor->listen_count) {
                reactor->listen_sockets[i] = reactor->listen_sockets[reactor->listen_count];
            }
            reactor->num_fds--;
            return 0;
        }
    }
    
    /* Cancel pending I/O operations on this socket
     * This will cause GetQueuedCompletionStatus to return
     * ERROR_OPERATION_ABORTED for any pending operations.
     */
    CancelIo((HANDLE)fd);
    
    /* Note: Contexts are freed when completion notifications arrive
     * with ERROR_OPERATION_ABORTED status
     */
    
    reactor->num_fds--;
    return 0;
}

/*
 * =============================================================================
 * Async I/O Operations
 * =============================================================================
 */

int io_reactor_post_read(io_reactor_t *reactor, socket_fd_t fd, void *buffer, size_t len) {
    if (!reactor || fd == INVALID_SOCKET) {
        return -1;
    }
    
    /* Allocate context for this read operation
     * We set user_context to NULL here because it should already be
     * associated with the socket via previous operations or will be
     * tracked separately by the application.
     */
    iocp_context_t *io_ctx = alloc_iocp_context(reactor, fd, NULL, OP_READ);
    if (!io_ctx) {
        return -1;
    }
    
    /* Use provided buffer or internal buffer */
    if (buffer && len > 0) {
        io_ctx->wsa_buf.buf = (char*)buffer;
        io_ctx->wsa_buf.len = (ULONG)len;
    }
    
    DWORD flags = 0;
    DWORD bytes_received = 0;
    
    int result = WSARecv(fd, &io_ctx->wsa_buf, 1, &bytes_received,
                        &flags, &io_ctx->overlapped, NULL);
    
    if (result == SOCKET_ERROR) {
        DWORD error = WSAGetLastError();
        if (error != WSA_IO_PENDING) {
            /* Immediate error */
            free_iocp_context(reactor, io_ctx);
            return -1;
        }
        /* WSA_IO_PENDING is expected for async operations */
    }
    
    return 0;
}

int io_reactor_post_write(io_reactor_t *reactor, socket_fd_t fd, void *buffer, size_t len) {
    if (!reactor || fd == INVALID_SOCKET || !buffer || len == 0) {
        return -1;
    }
    
    iocp_context_t *io_ctx = alloc_iocp_context(reactor, fd, NULL, OP_WRITE);
    if (!io_ctx) {
        return -1;
    }
    
    /* Copy data to internal buffer to ensure it remains valid
     * during the async operation
     */
    if (len > sizeof(io_ctx->buffer)) {
        len = sizeof(io_ctx->buffer);
    }
    memcpy(io_ctx->buffer, buffer, len);
    io_ctx->wsa_buf.buf = io_ctx->buffer;
    io_ctx->wsa_buf.len = (ULONG)len;
    
    DWORD bytes_sent = 0;
    
    int result = WSASend(fd, &io_ctx->wsa_buf, 1, &bytes_sent,
                        0, &io_ctx->overlapped, NULL);
    
    if (result == SOCKET_ERROR) {
        DWORD error = WSAGetLastError();
        if (error != WSA_IO_PENDING) {
            free_iocp_context(reactor, io_ctx);
            return -1;
        }
    }
    
    return 0;
}

/*
 * =============================================================================
 * Event Loop Integration
 * =============================================================================
 */

int io_reactor_wait(io_reactor_t *reactor, io_event_t *events,
                    int max_events, struct timeval *timeout) {
    if (!reactor || !events || max_events <= 0) {
        return -1;
    }
    
    DWORD timeout_ms;
    if (timeout == NULL) {
        timeout_ms = INFINITE;
    } else {
        timeout_ms = (DWORD)(timeout->tv_sec * 1000 + timeout->tv_usec / 1000);
    }
    
    int event_count = 0;
    
    /* Check console input first (fast, non-blocking) */
    if (reactor->console_enabled && event_count < max_events) {
        int console_result = check_console_input(reactor, &events[event_count]);
        if (console_result == 1) {
            event_count++;
        }
    }
    
    /* Check listening sockets for incoming connections using select() */
    if (reactor->listen_count > 0 && event_count < max_events) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        
        socket_fd_t max_fd = 0;
        for (int i = 0; i < reactor->listen_count; i++) {
            FD_SET(reactor->listen_sockets[i].fd, &read_fds);
            if (reactor->listen_sockets[i].fd > max_fd) {
                max_fd = reactor->listen_sockets[i].fd;
            }
        }
        
        /* Use zero timeout for non-blocking check */
        struct timeval zero_timeout = {0, 0};
        int ready = select((int)(max_fd + 1), &read_fds, NULL, NULL, &zero_timeout);
        
        if (ready > 0) {
            /* Check which listening sockets are ready */
            for (int i = 0; i < reactor->listen_count && event_count < max_events; i++) {
                if (FD_ISSET(reactor->listen_sockets[i].fd, &read_fds)) {
                    events[event_count].context = reactor->listen_sockets[i].context;
                    events[event_count].event_type = EVENT_READ;
                    events[event_count].bytes_transferred = 0;
                    events[event_count].buffer = NULL;
                    event_count++;
                }
            }
        }
    }
    
    /* Retrieve completed I/O operations */
    while (event_count < max_events) {
        DWORD bytes_transferred;
        ULONG_PTR completion_key;
        OVERLAPPED *overlapped;
        
        /* Only wait on first iteration; subsequent iterations check for more ready events */
        DWORD wait_time = (event_count == 0) ? timeout_ms : 0;
        
        BOOL result = GetQueuedCompletionStatus(
            reactor->iocp_handle,
            &bytes_transferred,
            &completion_key,
            &overlapped,
            wait_time
        );
        
        if (!result && overlapped == NULL) {
            /* Timeout or error with no completion packet */
            DWORD error = GetLastError();
            if (error == WAIT_TIMEOUT) {
                break;  /* Timeout - return events collected so far */
            }
            /* Other error */
            if (event_count == 0) {
                /* Only report error if we haven't collected any events */
                return -1;
            }
            /* If we have events, return them and ignore this error */
            break;
        }
        
        /* Get IOCP context from overlapped structure
         * CONTAINING_RECORD is a Windows macro that calculates the address
         * of the containing structure given a pointer to a member.
         */
        iocp_context_t *io_ctx = CONTAINING_RECORD(overlapped, iocp_context_t, overlapped);
        
        /* Populate event structure */
        events[event_count].context = io_ctx->user_context;
        events[event_count].bytes_transferred = (int)bytes_transferred;
        events[event_count].buffer = io_ctx->buffer;
        events[event_count].event_type = 0;
        
        if (!result) {
            /* Completion with error */
            DWORD error = GetLastError();
            if (error == ERROR_OPERATION_ABORTED) {
                /* I/O was cancelled (from io_reactor_remove or socket closure) */
                events[event_count].event_type = EVENT_CLOSE;
            } else if (error == ERROR_NETNAME_DELETED || error == ERROR_CONNECTION_ABORTED) {
                /* Connection closed abruptly */
                events[event_count].event_type = EVENT_CLOSE;
            } else {
                events[event_count].event_type = EVENT_ERROR;
            }
        } else if (bytes_transferred == 0 && io_ctx->operation == OP_READ) {
            /* Graceful close - zero-byte read indicates FIN received */
            events[event_count].event_type = EVENT_CLOSE;
        } else if (io_ctx->operation == OP_READ) {
            events[event_count].event_type = EVENT_READ;
        } else if (io_ctx->operation == OP_WRITE) {
            events[event_count].event_type = EVENT_WRITE;
        }
        
        /* Return context to pool */
        free_iocp_context(reactor, io_ctx);
        
        event_count++;
    }
    
    return event_count;
}

/*
 * =============================================================================
 * Console Support (Windows-specific)
 * =============================================================================
 */

/**
 * @brief Register Windows console input for event monitoring.
 *
 * Windows console I/O is not socket-based and cannot use IOCP. This function
 * enables polling of console input events in io_reactor_wait().
 *
 * @param reactor The reactor instance. Must not be NULL.
 * @param context User context pointer (returned in console events).
 * @return 0 on success, -1 on failure.
 */
int io_reactor_add_console(io_reactor_t *reactor, void *context) {
    if (!reactor) {
        return -1;
    }
    
    /* Get console input handle */
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE || hStdin == NULL) {
        return -1;
    }
   
    /* Store console state */
    reactor->console_handle = hStdin;
    reactor->console_context = context;
    reactor->console_enabled = 1;
    
    return 0;
}

/**
 * @brief Check if console has input available (called by io_reactor_wait).
 *
 * @param reactor The reactor instance.
 * @param event Event structure to fill if console input is available.
 * @return 1 if console event generated, 0 if no input available, -1 on error.
 */
static int check_console_input(io_reactor_t *reactor, io_event_t *event) {
    if (!reactor->console_enabled) {
        return 0;
    }
    
    DWORD num_events = 0;
    if (!GetNumberOfConsoleInputEvents(reactor->console_handle, &num_events)) {
        return -1;
    }
    
    if (num_events > 0) {
        /* Console has input available */
        event->context = reactor->console_context;
        event->event_type = EVENT_READ;
        event->buffer = NULL;
        event->bytes_transferred = 0;
        return 1;
    }
    
    return 0;
}
