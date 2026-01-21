/**
 * @file async_runtime_iocp.c
 * @brief Windows IOCP-based async runtime implementation
 * 
 * This implementation uses Windows I/O Completion Ports for unified handling
 * of both I/O events and worker thread completions.
 */

#ifdef _WIN32

/* Must include winsock2.h before windows.h to avoid conflicts */
#include <winsock2.h>
#include <windows.h>
#include "async/async_runtime.h"
#include <stdlib.h>
#include <string.h>

/* Maximum text buffer size */
#ifndef MAX_TEXT
#define MAX_TEXT 2048
#endif

/* Initial capacity for context pool */
#define INITIAL_POOL_SIZE 256

/* Operation types for IOCP contexts */
#define OP_READ    1
#define OP_WRITE   2
#define OP_ACCEPT  3

/* Completion keys for special events */
#define ACCEPT_COMPLETION_KEY  ((uintptr_t)-2)

/**
 * IOCP context for each I/O operation
 * The OVERLAPPED structure must be the first member
 */
typedef struct iocp_context_s {
    OVERLAPPED overlapped;
    void* user_context;
    int operation;
    WSABUF wsa_buf;
    char buffer[MAX_TEXT];
    socket_fd_t fd;
} iocp_context_t;

/**
 * Listening socket entry for accept worker
 */
typedef struct listening_socket_s {
    socket_fd_t fd;
    void* context;
} listening_socket_t;

/**
 * Async runtime implementation for Windows
 */
struct async_runtime_s {
    HANDLE iocp_handle;
    int num_fds;
    
    /* Context pool for allocation efficiency */
    iocp_context_t** context_pool;
    int pool_size;
    int pool_capacity;
    
    /* Accept worker for listening sockets */
    HANDLE accept_thread;
    DWORD accept_thread_id;
    CRITICAL_SECTION listen_lock;
    listening_socket_t* listen_sockets;
    int listen_count;
    int listen_capacity;
    volatile int accept_thread_running;
    
    /* Console support */
    console_type_t console_type;
    HANDLE console_handle;
    void* console_context;
    int console_enabled;
    iocp_context_t* console_read_ctx;
    
    /* Wakeup event for interrupting wait */
    HANDLE wakeup_event;
};

/* Context pool management */

static iocp_context_t* alloc_iocp_context(async_runtime_t* runtime, socket_fd_t fd,
                                          void* user_context, int operation) {
    iocp_context_t* ctx;
    
    if (runtime->pool_size > 0) {
        ctx = runtime->context_pool[--runtime->pool_size];
    } else {
        ctx = calloc(1, sizeof(iocp_context_t));
        if (!ctx) return NULL;
    }
    
    ZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));
    ctx->user_context = user_context;
    ctx->operation = operation;
    ctx->fd = fd;
    ctx->wsa_buf.buf = ctx->buffer;
    ctx->wsa_buf.len = sizeof(ctx->buffer);
    
    return ctx;
}

static void free_iocp_context(async_runtime_t* runtime, iocp_context_t* ctx) {
    if (!ctx) return;
    
    if (runtime->pool_size < runtime->pool_capacity) {
        runtime->context_pool[runtime->pool_size++] = ctx;
    } else {
        free(ctx);
    }
}

/* Accept worker thread - monitors listening sockets and posts accepted connections to IOCP */
static DWORD WINAPI accept_worker_thread(LPVOID param) {
    async_runtime_t* runtime = (async_runtime_t*)param;
    
    while (runtime->accept_thread_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        socket_fd_t max_fd = INVALID_SOCKET;
        
        /* Build fd_set from listening sockets */
        EnterCriticalSection(&runtime->listen_lock);
        for (int i = 0; i < runtime->listen_count; i++) {
            FD_SET(runtime->listen_sockets[i].fd, &read_fds);
            if (max_fd == INVALID_SOCKET || runtime->listen_sockets[i].fd > max_fd) {
                max_fd = runtime->listen_sockets[i].fd;
            }
        }
        int listen_count = runtime->listen_count;
        LeaveCriticalSection(&runtime->listen_lock);
        
        if (listen_count == 0) {
            /* No listening sockets, sleep briefly */
            Sleep(100);
            continue;
        }
        
        /* Wait for activity with 1 second timeout */
        struct timeval timeout = {1, 0};
        int result = select((int)max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (result > 0) {
            /* Check each listening socket */
            EnterCriticalSection(&runtime->listen_lock);
            for (int i = 0; i < runtime->listen_count; i++) {
                socket_fd_t listen_fd = runtime->listen_sockets[i].fd;
                void* context = runtime->listen_sockets[i].context;
                
                if (FD_ISSET(listen_fd, &read_fds)) {
                    LeaveCriticalSection(&runtime->listen_lock);
                    
                    /* Accept connection (non-blocking) */
                    struct sockaddr_in addr;
                    int addr_len = sizeof(addr);
                    socket_fd_t accepted_fd = accept(listen_fd, (struct sockaddr*)&addr, &addr_len);
                    
                    if (accepted_fd != INVALID_SOCKET) {
                        /* Post completion to IOCP with listening socket context */
                        PostQueuedCompletionStatus(runtime->iocp_handle,
                                                  (DWORD)(uintptr_t)accepted_fd,
                                                  (ULONG_PTR)context,
                                                  NULL);
                    }
                    
                    EnterCriticalSection(&runtime->listen_lock);
                }
            }
            LeaveCriticalSection(&runtime->listen_lock);
        }
    }
    
    return 0;
}

/* Lifecycle management */

async_runtime_t* async_runtime_init(void) {
    async_runtime_t* runtime = calloc(1, sizeof(async_runtime_t));
    if (!runtime) return NULL;
    
    /* Create IOCP with 1 concurrent thread (single-threaded model) */
    runtime->iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if (!runtime->iocp_handle) {
        free(runtime);
        return NULL;
    }
    
    /* Initialize context pool */
    runtime->pool_capacity = INITIAL_POOL_SIZE;
    runtime->context_pool = calloc(runtime->pool_capacity, sizeof(iocp_context_t*));
    if (!runtime->context_pool) {
        CloseHandle(runtime->iocp_handle);
        free(runtime);
        return NULL;
    }
    
    /* Initialize listening socket array and accept worker */
    runtime->listen_capacity = 8;
    runtime->listen_sockets = calloc(runtime->listen_capacity, sizeof(listening_socket_t));
    if (!runtime->listen_sockets) {
        free(runtime->context_pool);
        CloseHandle(runtime->iocp_handle);
        free(runtime);
        return NULL;
    }
    
    InitializeCriticalSection(&runtime->listen_lock);
    runtime->accept_thread_running = 1;
    runtime->accept_thread = CreateThread(NULL, 0, accept_worker_thread, runtime, 0, &runtime->accept_thread_id);
    if (!runtime->accept_thread) {
        DeleteCriticalSection(&runtime->listen_lock);
        free(runtime->listen_sockets);
        free(runtime->context_pool);
        CloseHandle(runtime->iocp_handle);
        free(runtime);
        return NULL;
    }
    
    /* Initialize console support */
    runtime->console_handle = INVALID_HANDLE_VALUE;
    runtime->console_enabled = 0;
    runtime->console_read_ctx = NULL;
    
    /* Create wakeup event */
    runtime->wakeup_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!runtime->wakeup_event) {
        free(runtime->listen_sockets);
        free(runtime->context_pool);
        CloseHandle(runtime->iocp_handle);
        free(runtime);
        return NULL;
    }
    
    return runtime;
}

void async_runtime_deinit(async_runtime_t* runtime) {
    if (!runtime) return;
    
    /* Stop accept worker thread */
    if (runtime->accept_thread) {
        runtime->accept_thread_running = 0;
        WaitForSingleObject(runtime->accept_thread, 5000);
        CloseHandle(runtime->accept_thread);
    }
    
    DeleteCriticalSection(&runtime->listen_lock);
    free(runtime->listen_sockets);
    
    /* Free context pool */
    for (int i = 0; i < runtime->pool_size; i++) {
        free(runtime->context_pool[i]);
    }
    free(runtime->context_pool);
    
    if (runtime->console_read_ctx) {
        free(runtime->console_read_ctx);
    }
    
    if (runtime->iocp_handle && runtime->iocp_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(runtime->iocp_handle);
    }
    
    if (runtime->wakeup_event) {
        CloseHandle(runtime->wakeup_event);
    }
    
    free(runtime);
}

/* I/O source management */

int async_runtime_add(async_runtime_t* runtime, socket_fd_t fd, uint32_t events, void* context) {
    if (!runtime || fd == INVALID_SOCKET) return -1;
    
    /* Check if this is a listening socket */
    BOOL is_listening = FALSE;
    int optlen = sizeof(is_listening);
    if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, (char*)&is_listening, &optlen) == 0 && is_listening) {
        /* Listening socket - add to accept worker's monitoring list */
        EnterCriticalSection(&runtime->listen_lock);
        
        /* Grow array if needed */
        if (runtime->listen_count >= runtime->listen_capacity) {
            int new_capacity = runtime->listen_capacity * 2;
            listening_socket_t* new_array = realloc(runtime->listen_sockets,
                                                   new_capacity * sizeof(listening_socket_t));
            if (!new_array) {
                LeaveCriticalSection(&runtime->listen_lock);
                return -1;
            }
            runtime->listen_sockets = new_array;
            runtime->listen_capacity = new_capacity;
        }
        
        /* Add to array */
        runtime->listen_sockets[runtime->listen_count].fd = fd;
        runtime->listen_sockets[runtime->listen_count].context = context;
        runtime->listen_count++;
        
        LeaveCriticalSection(&runtime->listen_lock);
        
        runtime->num_fds++;
        return 0;
    }
    
    /* Associate socket with IOCP */
    HANDLE result = CreateIoCompletionPort((HANDLE)fd, runtime->iocp_handle,
                                          (ULONG_PTR)context, 0);
    if (!result) return -1;
    
    /* Post initial async read if requested */
    if (events & EVENT_READ) {
        iocp_context_t* io_ctx = alloc_iocp_context(runtime, fd, NULL, OP_READ);
        if (!io_ctx) return -1;
        
        DWORD flags = 0;
        DWORD bytes_received;
        int wsa_result = WSARecv(fd, &io_ctx->wsa_buf, 1, &bytes_received, &flags,
                                 &io_ctx->overlapped, NULL);
        
        if (wsa_result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            free_iocp_context(runtime, io_ctx);
            return -1;
        }
    }
    
    runtime->num_fds++;
    return 0;
}

int async_runtime_modify(async_runtime_t* runtime, socket_fd_t fd, uint32_t events, void* context) {
    /* IOCP doesn't need explicit modify - just post new operations */
    (void)runtime; (void)fd; (void)events; (void)context;
    return 0;
}

int async_runtime_remove(async_runtime_t* runtime, socket_fd_t fd) {
    if (!runtime) return -1;
    
    /* Check if this is a listening socket */
    EnterCriticalSection(&runtime->listen_lock);
    for (int i = 0; i < runtime->listen_count; i++) {
        if (runtime->listen_sockets[i].fd == fd) {
            /* Remove from array (swap with last element) */
            runtime->listen_count--;
            if (i < runtime->listen_count) {
                runtime->listen_sockets[i] = runtime->listen_sockets[runtime->listen_count];
            }
            
            LeaveCriticalSection(&runtime->listen_lock);
            runtime->num_fds--;
            return 0;
        }
    }
    LeaveCriticalSection(&runtime->listen_lock);
    
    /* Not a listening socket - just decrement count */
    runtime->num_fds--;
    return 0;
}

int async_runtime_wakeup(async_runtime_t* runtime) {
    if (!runtime || !runtime->wakeup_event) return -1;
    return SetEvent(runtime->wakeup_event) ? 0 : -1;
}

/* Event loop */

int async_runtime_wait(async_runtime_t* runtime, io_event_t* events,
                       int max_events, struct timeval* timeout) {
    if (!runtime || !events || max_events <= 0) return -1;
    
    DWORD timeout_ms = INFINITE;
    if (timeout) {
        timeout_ms = (timeout->tv_sec * 1000) + (timeout->tv_usec / 1000);
    }
    
    int event_count = 0;
    
    /* All events (connected sockets, accepted connections, worker completions)
     * flow through IOCP - single blocking point, no polling */
    OVERLAPPED_ENTRY entries[64];
    ULONG num_entries = 0;
    
    if (GetQueuedCompletionStatusEx(runtime->iocp_handle, entries, 64,
                                    &num_entries, timeout_ms, FALSE)) {
        /* Process IOCP completions */
        for (ULONG i = 0; i < num_entries && event_count < max_events; i++) {
            iocp_context_t* io_ctx = (iocp_context_t*)entries[i].lpOverlapped;
            
            if (io_ctx) {
                /* Connected socket I/O completion */
                events[event_count].fd = io_ctx->fd;
                events[event_count].handle = NULL;
                events[event_count].completion_key = entries[i].lpCompletionKey;
                events[event_count].context = (void*)entries[i].lpCompletionKey;
                events[event_count].bytes_transferred = entries[i].dwNumberOfBytesTransferred;
                events[event_count].buffer = io_ctx->buffer;
                
                if (io_ctx->operation == OP_READ) {
                    events[event_count].event_type = (entries[i].dwNumberOfBytesTransferred > 0)
                                                     ? EVENT_READ : EVENT_CLOSE;
                } else if (io_ctx->operation == OP_WRITE) {
                    events[event_count].event_type = EVENT_WRITE;
                }
                free_iocp_context(runtime, io_ctx);
            } else {
                /* NULL overlapped - either worker completion or accepted connection */
                /* Accept worker posts accepted FD in dwNumberOfBytesTransferred */
                socket_fd_t accepted_fd = (socket_fd_t)entries[i].dwNumberOfBytesTransferred;
                
                events[event_count].fd = accepted_fd;
                events[event_count].handle = NULL;
                events[event_count].completion_key = entries[i].lpCompletionKey;
                events[event_count].context = (void*)entries[i].lpCompletionKey;
                events[event_count].event_type = EVENT_READ;  /* Listening socket ready or worker completion */
                events[event_count].bytes_transferred = 0;
                events[event_count].buffer = NULL;
            }
            
            event_count++;
        }
    }
    
    return event_count;
}

int async_runtime_post_completion(async_runtime_t* runtime, uintptr_t completion_key, uintptr_t data) {
    if (!runtime || !runtime->iocp_handle) return -1;
    
    BOOL result = PostQueuedCompletionStatus(
        runtime->iocp_handle,
        (DWORD)data,
        completion_key,
        NULL  /* NULL overlapped indicates worker completion */
    );
    
    return result ? 0 : -1;
}

int async_runtime_post_read(async_runtime_t* runtime, socket_fd_t fd, void* buffer, size_t len) {
    if (!runtime || fd == INVALID_SOCKET) return -1;
    
    iocp_context_t* io_ctx = alloc_iocp_context(runtime, fd, NULL, OP_READ);
    if (!io_ctx) return -1;
    
    if (buffer && len > 0) {
        io_ctx->wsa_buf.buf = (char*)buffer;
        io_ctx->wsa_buf.len = (ULONG)len;
    }
    
    DWORD flags = 0;
    DWORD bytes_received;
    int result = WSARecv(fd, &io_ctx->wsa_buf, 1, &bytes_received, &flags,
                         &io_ctx->overlapped, NULL);
    
    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        free_iocp_context(runtime, io_ctx);
        return -1;
    }
    
    return 0;
}

int async_runtime_post_write(async_runtime_t* runtime, socket_fd_t fd, void* buffer, size_t len) {
    if (!runtime || fd == INVALID_SOCKET || !buffer) return -1;
    
    iocp_context_t* io_ctx = alloc_iocp_context(runtime, fd, NULL, OP_WRITE);
    if (!io_ctx) return -1;
    
    io_ctx->wsa_buf.buf = (char*)buffer;
    io_ctx->wsa_buf.len = (ULONG)len;
    
    DWORD bytes_sent;
    int result = WSASend(fd, &io_ctx->wsa_buf, 1, &bytes_sent, 0,
                         &io_ctx->overlapped, NULL);
    
    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        free_iocp_context(runtime, io_ctx);
        return -1;
    }
    
    return 0;
}

/* Console support */

int async_runtime_add_console(async_runtime_t* runtime, void* context) {
    if (!runtime) return -1;
    
    runtime->console_handle = GetStdHandle(STD_INPUT_HANDLE);
    if (runtime->console_handle == INVALID_HANDLE_VALUE) return -1;
    
    runtime->console_context = context;
    runtime->console_enabled = 1;
    
    /* Determine console type */
    DWORD mode;
    if (GetConsoleMode(runtime->console_handle, &mode)) {
        runtime->console_type = CONSOLE_TYPE_REAL;
    } else {
        DWORD type = GetFileType(runtime->console_handle);
        runtime->console_type = (type == FILE_TYPE_PIPE) ? CONSOLE_TYPE_PIPE : CONSOLE_TYPE_FILE;
    }
    
    return 0;
}

console_type_t async_runtime_get_console_type(async_runtime_t* runtime) {
    return runtime ? runtime->console_type : CONSOLE_TYPE_NONE;
}

HANDLE async_runtime_get_console_event(async_runtime_t* runtime) {
    return (runtime && runtime->console_read_ctx) ? runtime->console_read_ctx->overlapped.hEvent : NULL;
}

void* async_runtime_get_console_ctx(async_runtime_t* runtime) {
    return runtime ? runtime->console_read_ctx : NULL;
}

HANDLE async_runtime_get_iocp(async_runtime_t* runtime) {
    return runtime ? runtime->iocp_handle : NULL;
}

#endif /* _WIN32 */
