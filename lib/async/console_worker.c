/**
 * @file console_worker.c
 * @brief Console input worker implementation
 *
 * Platform-agnostic console input handling via worker thread.
 */

#include "console_worker.h"
#include "port/debug.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <errno.h>
#endif

/**
 * Convert console type to string
 */
const char* console_type_str(console_type_t type) {
    switch (type) {
        case CONSOLE_TYPE_NONE: return "NONE";
        case CONSOLE_TYPE_REAL: return "REAL";
        case CONSOLE_TYPE_PIPE: return "PIPE";
        case CONSOLE_TYPE_FILE: return "FILE";
        default: return "UNKNOWN";
    }
}

/**
 * Detect console type (platform-specific)
 */
console_type_t console_detect_type(void) {
#ifdef _WIN32
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE || hStdin == NULL) {
        return CONSOLE_TYPE_NONE;
    }

    DWORD mode;
    if (GetConsoleMode(hStdin, &mode)) {
        /* Real Windows console */
        return CONSOLE_TYPE_REAL;
    }

    /* Not a console - check if pipe or file */
    DWORD file_type = GetFileType(hStdin);
    if (file_type == FILE_TYPE_PIPE) {
        return CONSOLE_TYPE_PIPE;
    } else if (file_type == FILE_TYPE_DISK) {
        return CONSOLE_TYPE_FILE;
    }

    return CONSOLE_TYPE_NONE;
#else
    /* POSIX: use isatty */
    if (isatty(STDIN_FILENO)) {
        return CONSOLE_TYPE_REAL;
    }

    /* Check if pipe or file via stat */
    struct stat st;
    if (fstat(STDIN_FILENO, &st) == 0) {
        if (S_ISFIFO(st.st_mode)) {
            return CONSOLE_TYPE_PIPE;
        } else if (S_ISREG(st.st_mode)) {
            return CONSOLE_TYPE_FILE;
        }
    }

    return CONSOLE_TYPE_NONE;
#endif
}

#ifdef _WIN32
/**
 * Windows console worker thread procedure
 */
static void* console_worker_proc_win32(void* ctx) {
    console_worker_context_t* cctx = (console_worker_context_t*)ctx;
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    
    /* Create shutdown event for graceful termination of blocking I/O */
    HANDLE hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!hShutdownEvent) {
        debug_error("Failed to create shutdown event: %lu\n", GetLastError());
        return NULL;
    }
    
    /* Set UTF-8 code page */
    SetConsoleCP(CP_UTF8);

    /* If standard input is a console, enable line input and processed input modes */
    DWORD mode;
    if (GetConsoleMode(hStdin, &mode)) {
        DWORD new_mode = ENABLE_EXTENDED_FLAGS
            | ENABLE_QUICK_EDIT_MODE    /* allows mouse select and edit of console input */
            | ENABLE_PROCESSED_INPUT    /* Ctrl-C handling, plus other control keys */
            | ENABLE_LINE_INPUT         /* ReadConsoleA() returns only when ENTER is pressed */
            | ENABLE_ECHO_INPUT         /* echo input characters */
            ;
        if (!SetConsoleMode(hStdin, new_mode)) {
            debug_warn ("SetConsoleMode failed to enable line input: %lu\n", GetLastError());
        }
    }

    char line_buffer[CONSOLE_MAX_LINE];
    DWORD chars_read = 0;

    debug_info ("Console worker started (type: %s)\n", console_type_str(cctx->console_type));

    while (!async_worker_should_stop(async_worker_current())) {
        /* Use overlapped I/O for all types to allow cancellation */
        OVERLAPPED overlapped = {0};
        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!overlapped.hEvent) {
            debug_error ("Failed to create event for overlapped I/O\n");
            break;
        }

        BOOL result;
        if (cctx->console_type == CONSOLE_TYPE_REAL) {
            /* Real console: ReadConsoleA with overlapped I/O */
            result = ReadConsoleA(hStdin, line_buffer, CONSOLE_MAX_LINE - 1, NULL, &overlapped);
        } else {
            /* Pipe or file: Use ReadFile with overlapped I/O for cancelability */
            result = ReadFile(hStdin, line_buffer, CONSOLE_MAX_LINE - 1, NULL, &overlapped);
        }

        if (!result && GetLastError() != ERROR_IO_PENDING) {
            debug_error ("Read failed: %lu\n", GetLastError());
            CloseHandle(overlapped.hEvent);
            break;
        }

        /* Wait for either I/O completion or shutdown signal */
        HANDLE events[2] = { overlapped.hEvent, hShutdownEvent };
        DWORD wait_result = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        debug_trace ("Console worker wait result: %lu\n", wait_result);
        
        if (wait_result == WAIT_OBJECT_0) {
            /* I/O completed successfully */
            if (!GetOverlappedResult(hStdin, &overlapped, &chars_read, FALSE)) {
                DWORD err = GetLastError();
                if (err != ERROR_OPERATION_ABORTED) {
                    debug_error ("GetOverlappedResult failed: %lu\n", err);
                }
                CloseHandle(overlapped.hEvent);
                break;
            }
            CloseHandle(overlapped.hEvent);
            
            if (chars_read > 0) {
                /* Null-terminate */
                line_buffer[chars_read] = '\0';

                /* Enqueue line */
                if (!async_queue_enqueue(cctx->line_queue, line_buffer, chars_read + 1)) {
                    debug_warn ("Console line queue full, dropping line\n");
                }

                /* Post completion to wake main thread */
                async_runtime_post_completion(cctx->runtime, cctx->completion_key, chars_read);
            }
        } else if (wait_result == WAIT_OBJECT_0 + 1) {
            /* Shutdown signaled - cancel pending I/O and exit cleanly */
            debug_info("Console worker shutdown signaled, canceling I/O\n");
            CancelIoEx(hStdin, &overlapped);
            
            /* Wait for cancellation to complete */
            GetOverlappedResult(hStdin, &overlapped, &chars_read, TRUE);
            CloseHandle(overlapped.hEvent);
            break;
        } else {
            /* Error or unexpected result */
            debug_error ("WaitForMultipleObjects failed: %lu\n", wait_result);
            CancelIoEx(hStdin, &overlapped);
            CloseHandle(overlapped.hEvent);
            break;
        }
        
        /* Check shutdown flag after processing (redundant but safe) */
        if (async_worker_should_stop(async_worker_current())) {
            SetEvent(hShutdownEvent); /* Ensure event is set for next iteration */
        }
    }

    CloseHandle(hShutdownEvent);
    debug_info ("Console worker stopped\n");
    return NULL;
}
#else
/**
 * POSIX console worker thread procedure
 */
static void* console_worker_proc_posix(void* ctx) {
    console_worker_context_t* cctx = (console_worker_context_t*)ctx;
    char line_buffer[CONSOLE_MAX_LINE];

    debug_message("Console worker started (type: %s)\n", console_type_str(cctx->console_type));

    while (!async_worker_should_stop(async_worker_current())) {
        /* Use select with timeout to check shutdown flag */
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000; /* 10ms */

        int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
        if (ret < 0) {
            if (errno == EINTR) {
                continue; /* Interrupted by signal, retry */
            }
            debug_message("select() failed: %s\n", strerror(errno));
            break;
        } else if (ret == 0) {
            /* Timeout - check shutdown flag */
            continue;
        }

        /* stdin is readable */
        ssize_t bytes_read = read(STDIN_FILENO, line_buffer, CONSOLE_MAX_LINE - 1);
        if (bytes_read < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            debug_message("read() failed: %s\n", strerror(errno));
            break;
        } else if (bytes_read == 0) {
            /* EOF */
            debug_message("Console EOF detected\n");
            break;
        }

        /* Null-terminate */
        line_buffer[bytes_read] = '\0';

        /* Enqueue line */
        if (!async_queue_enqueue(cctx->line_queue, line_buffer, bytes_read + 1)) {
            debug_message("Console line queue full, dropping line\n");
        }

        /* Post completion to wake main thread */
        async_runtime_post_completion(cctx->runtime, cctx->completion_key, bytes_read);
    }

    debug_message("Console worker stopped\n");
    return NULL;
}
#endif

/**
 * Initialize console worker
 */
console_worker_context_t* console_worker_init(async_runtime_t* runtime, async_queue_t* queue, uintptr_t completion_key) {
    if (!runtime || !queue) {
        debug_error ("console_worker_init: invalid arguments\n");
        return NULL;
    }

    console_worker_context_t* ctx = (console_worker_context_t*)calloc(1, sizeof(*ctx));
    if (!ctx) {
        debug_error ("console_worker_init: out of memory\n");
        return NULL;
    }

    ctx->line_queue = queue;
    ctx->runtime = runtime;
    ctx->completion_key = completion_key;
    ctx->console_type = console_detect_type();

    /* debug_info ("Console type detected: %s\n", console_type_str(ctx->console_type)); */

    if (ctx->console_type == CONSOLE_TYPE_NONE) {
        debug_warn ("No console detected, worker will not start\n");
        /* Don't treat as fatal - allow mudlib to run without console */
        return ctx;
    }

#ifdef _WIN32
    ctx->worker = async_worker_create(console_worker_proc_win32, ctx, 0);
#else
    ctx->worker = async_worker_create(console_worker_proc_posix, ctx, 0);
#endif

    if (!ctx->worker) {
        debug_error ("Failed to create console worker thread\n");
        free(ctx);
        return NULL;
    }

    return ctx;
}

/**
 * Shutdown console worker
 */
bool console_worker_shutdown(console_worker_context_t* ctx, int timeout_ms) {
    if (!ctx || !ctx->worker) {
        return true; /* No worker to shutdown */
    }

    async_worker_signal_stop(ctx->worker);
    return async_worker_join(ctx->worker, timeout_ms);
}

/**
 * Destroy console worker
 */
void console_worker_destroy(console_worker_context_t* ctx) {
    if (!ctx) {
        return;
    }

    if (ctx->worker) {
        async_worker_destroy(ctx->worker);
    }

    free(ctx);
}
