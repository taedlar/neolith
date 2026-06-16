/**
 * @file console_worker.c
 * @brief Console input worker implementation
 *
 * Platform-agnostic console input handling via worker thread.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#define NO_STEM
#include "src/std.h"
#include "console_worker.h"
#include "console_mode.h"
#include "port/debug.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>

static int restore_desired_console_input_mode (console_worker_context_t* ctx) {
    DWORD mode;

    if (!ctx)
        return 0;

    platform_mutex_lock(&ctx->state_mutex);
    mode = (DWORD)ctx->desired_console_mode;
    platform_mutex_unlock(&ctx->state_mutex);

    if (mode & ENABLE_LINE_INPUT)
        return set_console_input_line_mode(ctx, (mode & ENABLE_ECHO_INPUT) != 0);

    return set_console_input_single_char(ctx, 1);
}
#else
#include <unistd.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <errno.h>
#endif

static void console_worker_set_eof(console_worker_context_t* ctx) {
    if (!ctx)
        return;

    platform_mutex_lock(&ctx->state_mutex);
    ctx->eof_detected = true;
    platform_mutex_unlock(&ctx->state_mutex);

    /* Wake the main loop so EOF can be handled on the main thread. */
    async_runtime_post_completion(ctx->runtime, ctx->completion_key, 0);
}

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
    platform_event_t* stop_event = async_worker_get_stop_event(async_worker_current());
    
    if (!stop_event) {
        debug_error("Failed to get stop event\n");
        return NULL;
    }
    
    HANDLE hStopEvent = (HANDLE)platform_event_get_native_handle(stop_event);
    if (!hStopEvent) {
        debug_error("Failed to get native event handle\n");
        return NULL;
    }
    
    /* Set UTF-8 code page */
    SetConsoleCP(CP_UTF8);

    /* If standard input is a console, enable cooked line input with echo. */
    restore_desired_console_input_mode(cctx);

    char line_buffer[CONSOLE_MAX_LINE];
    WCHAR wide_buffer[CONSOLE_MAX_LINE];
    DWORD chars_read = 0;

    debug_notice ("console worker started (type: %s)\n", console_type_str(cctx->console_type));

    while (!async_worker_should_stop(async_worker_current())) {
        /* Wait for stdin to be signaled OR stop event */
        HANDLE events[2] = { hStdin, hStopEvent };
        DWORD wait_result = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        
        if (wait_result == WAIT_OBJECT_0) {
            /* stdin is signaled - data available, read it synchronously */
            BOOL result;
            if (cctx->console_type == CONSOLE_TYPE_REAL) {
                /* Real console: ReadConsoleW (Unicode).
                 * dwCtrlWakeupMask is only honored by ReadConsoleW, not ReadConsoleA.
                 * Bit 27 (ESC = 0x1B) lets ESCAPE unblock ReadConsole in cooked mode.
                 */
                CONSOLE_READCONSOLE_CONTROL rdcon;
                rdcon.nLength = sizeof(rdcon);
                rdcon.nInitialChars = 0;
                rdcon.dwCtrlWakeupMask = (1UL << 27); /* ESC = ASCII 27 */
                rdcon.dwControlKeyState = 0;
                DWORD wchars_read = 0;
                result = ReadConsoleW(hStdin, wide_buffer, CONSOLE_MAX_LINE - 1, &wchars_read, &rdcon);
                if (result && wchars_read > 0) {
                    /* ESC wakeup: last wide char is ESC - discard the partial line */
                    if (wide_buffer[wchars_read - 1] == L'\x1B')
                        continue;
                    /* Convert UTF-16 → UTF-8 into line_buffer */
                    int nb = WideCharToMultiByte (CP_UTF8, 0, wide_buffer, (int)wchars_read,
                                                 line_buffer, CONSOLE_MAX_LINE - 1, NULL, NULL);
                    chars_read = (nb > 0) ? (DWORD)nb : 0;
                }
                else if (result) {
                    /* ReadConsoleW returned TRUE with 0 chars: spurious wakeup from an
                     * injected input event (e.g. VK_ESCAPE key-up during mode switch).
                     * This is not EOF; loop back and wait for real input. */
                    LOG_INFO ("Console woken with no chars (spurious wakeup), continuing\n");
                    continue;
                }
                else {
                    chars_read = 0;
                }
            } else {
                /* Pipe or file: Use ReadFile (synchronous) */
                result = ReadFile (hStdin, line_buffer, CONSOLE_MAX_LINE - 1, &chars_read, NULL);
            }
            LOG_INFO ("Read %lu chars from console (result: %s)\n", chars_read, result ? "success" : "failure");

            if (!result) {
                DWORD err = GetLastError();
                if (err == ERROR_OPERATION_ABORTED) {
                    LOG_INFO ("Console read interrupted by shutdown or input mode switch\n");
                    restore_desired_console_input_mode (cctx);
                    continue;
                }
                if (cctx->console_type == CONSOLE_TYPE_REAL) {
                    /* SetConsoleMode changing ENABLE_LINE_INPUT while ReadConsoleW is blocking
                     * can return errors such as ERROR_INVALID_FUNCTION; treat these as
                     * non-fatal mode-switch interruptions rather than killing the thread. */
                    LOG_WARN ("ReadConsoleW interrupted (err=%lu), restoring mode and continuing\n", err);
                    restore_desired_console_input_mode (cctx);
                    continue;
                }
                LOG_ERROR ("Read failed: %lu\n", err);
                break;
            }
            
            if (chars_read > 0) {
                /* Null-terminate */
                line_buffer[chars_read] = '\0';

                /* Enqueue line */
                if (!async_queue_enqueue(cctx->line_queue, line_buffer, chars_read + 1)) {
                    LOG_WARN ("Console line queue full, dropping line\n");
                }

                /* Post completion to wake main thread */
                async_runtime_post_completion(cctx->runtime, cctx->completion_key, chars_read);
            } else {
                /* EOF */
                console_worker_set_eof(cctx);
                break;
            }
        } else if (wait_result == WAIT_OBJECT_0 + 1) {
            /* Stop event signaled - exit cleanly */
            break;
        } else {
            /* Error or unexpected result */
            LOG_ERROR ("WaitForMultipleObjects failed: %lu (error: %lu)\n", wait_result, GetLastError());
            break;
        }
    }

    LOG_INFO ("console worker stopped\n");
    return NULL;
}
#else
/**
 * POSIX console worker thread procedure
 */
static void* console_worker_proc_posix(void* ctx) {
    console_worker_context_t* cctx = (console_worker_context_t*)ctx;
    char line_buffer[CONSOLE_MAX_LINE];
    int stop_fd = cctx->stop_pipe_fds[0];

    debug_notice ("console worker started (type: %s)\n", console_type_str(cctx->console_type));

    while (!async_worker_should_stop(async_worker_current())) {
        /* Block in select() on stdin and the stop-pipe read end.
         * NULL timeout means infinite wait - no polling needed. */
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int nfds = STDIN_FILENO + 1;
        struct timeval poll_timeout;
        struct timeval* timeout_ptr = NULL; /* NULL = infinite, used when pipe is available */
        if (stop_fd >= 0) {
            FD_SET(stop_fd, &readfds);
            if (stop_fd >= nfds)
                nfds = stop_fd + 1;
        } else {
            /* Pipe unavailable (creation failed at init): fall back to 10ms polling
             * so the should_stop flag check at the top of the loop is eventually reached. */
            poll_timeout.tv_sec = 0;
            poll_timeout.tv_usec = 10000;
            timeout_ptr = &poll_timeout;
        }

        int ret = select(nfds, &readfds, NULL, NULL, timeout_ptr);
        if (ret < 0) {
            if (errno == EINTR) {
                continue; /* Interrupted by signal, retry */
            }
            debug_message("select() failed: %s\n", strerror(errno));
            break;
        }

        /* Stop pipe signaled - exit cleanly */
        if (stop_fd >= 0 && FD_ISSET(stop_fd, &readfds)) {
            break;
        }

        if (!FD_ISSET(STDIN_FILENO, &readfds)) {
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
            console_worker_set_eof(cctx);
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

    debug_notice ("console worker stopped\n");
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
    ctx->eof_detected = false;
#ifdef _WIN32
    ctx->desired_console_mode = ENABLE_EXTENDED_FLAGS
                                | ENABLE_QUICK_EDIT_MODE
                                | ENABLE_PROCESSED_INPUT
                                | ENABLE_LINE_INPUT
                                | ENABLE_ECHO_INPUT;
#endif

    if (!platform_mutex_init(&ctx->state_mutex)) {
        free(ctx);
        return NULL;
    }

    /* debug_notice ("Console type detected: %s\n", console_type_str(ctx->console_type)); */

#ifndef _WIN32
    /* Create self-pipe for stop signaling so the worker can block in select() indefinitely
     * and be woken by either stdin becoming readable or a stop signal. */
    ctx->stop_pipe_fds[0] = ctx->stop_pipe_fds[1] = -1;
    if (pipe(ctx->stop_pipe_fds) != 0) {
        debug_warn ("console_worker_init: failed to create stop pipe: %s\n", strerror(errno));
        /* Non-fatal: worker falls back to polling with timeout */
    }
#endif

    if (ctx->console_type == CONSOLE_TYPE_NONE) {
        debug_warn ("No console detected, worker will not start\n");
        /* Don't treat as fatal - allow mudlib to run without console */
        return ctx;
    }

#ifdef _WIN32
    ctx->worker = async_worker_create (console_worker_proc_win32, ctx, 0);
#else
    ctx->worker = async_worker_create (console_worker_proc_posix, ctx, 0);
#endif

    if (!ctx->worker) {
        debug_error ("Failed to create console worker thread\n");
#ifndef _WIN32
        if (ctx->stop_pipe_fds[0] >= 0) close(ctx->stop_pipe_fds[0]);
        if (ctx->stop_pipe_fds[1] >= 0) close(ctx->stop_pipe_fds[1]);
#endif
        platform_mutex_destroy(&ctx->state_mutex);
        free(ctx);
        return NULL;
    }

    return ctx;
}

/**
 * Shutdown console worker
 */
bool console_worker_shutdown(console_worker_context_t* ctx, int timeout_ms) {
    if (ctx && ctx->worker) {
        async_worker_signal_stop (ctx->worker); /* signal the stop event first */
        set_console_input_single_char (ctx, 1); /* interrupt line mode console read */
    }
    else
        return true; /* No worker to shutdown */

#ifndef _WIN32
    /* Wake the select() in the POSIX worker by writing to the stop pipe */
    if (ctx->stop_pipe_fds[1] >= 0) {
        char byte = 1;
        ssize_t written;
        do {
            written = write(ctx->stop_pipe_fds[1], &byte, 1);
        } while (written < 0 && errno == EINTR);
    }
#endif

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

#ifndef _WIN32
    if (ctx->stop_pipe_fds[0] >= 0) {
        close(ctx->stop_pipe_fds[0]);
        ctx->stop_pipe_fds[0] = -1;
    }
    if (ctx->stop_pipe_fds[1] >= 0) {
        close(ctx->stop_pipe_fds[1]);
        ctx->stop_pipe_fds[1] = -1;
    }
#endif

    platform_mutex_destroy(&ctx->state_mutex);

    free(ctx);
}

bool console_worker_take_eof(console_worker_context_t* ctx) {
    bool saw_eof;

    if (!ctx)
        return false;

    platform_mutex_lock(&ctx->state_mutex);
    saw_eof = ctx->eof_detected;
    ctx->eof_detected = false;
    platform_mutex_unlock(&ctx->state_mutex);

    return saw_eof;
}
