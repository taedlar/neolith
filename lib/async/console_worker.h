/**
 * @file console_worker.h
 * @brief Console input worker for non-blocking console I/O
 *
 * Platform-agnostic console input handling via worker thread.
 * - Windows: Detects REAL/PIPE/FILE console types, uses ReadConsole for native line editing
 * - POSIX: Uses read() on STDIN_FILENO with termios canonical mode
 *
 * Worker posts completions to async_runtime, main thread dequeues from async_queue.
 *
 * Implementation: docs/history/agent-reports/async-phase2-console-worker-2026-01-20.md
 */

#ifndef CONSOLE_WORKER_H
#define CONSOLE_WORKER_H

#include "async/async_queue.h"
#include "async/async_runtime.h"
#include "async/async_worker.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Console types (platform-specific)
 * 
 * Note: On Windows, this is already defined in async_runtime.h
 * On POSIX, we define it here for consistency
 */
#ifndef _WIN32
typedef enum {
    CONSOLE_TYPE_NONE = 0,     /**< Not a console (e.g., detached process) */
    CONSOLE_TYPE_REAL = 1,     /**< Real interactive console (Windows) or TTY (POSIX) */
    CONSOLE_TYPE_PIPE = 2,     /**< Piped stdin (e.g., testbot.py) */
    CONSOLE_TYPE_FILE = 3      /**< Redirected from file */
} console_type_t;
#endif

/**
 * Console worker context
 */
typedef struct console_worker_context_s {
    async_queue_t* line_queue;     /**< Queue for completed lines */
    async_runtime_t* runtime;      /**< Runtime for posting completions */
    async_worker_t* worker;        /**< Worker thread handle */
    console_type_t console_type;   /**< Detected console type */
    uintptr_t completion_key;      /**< Completion key for runtime */
} console_worker_context_t;

/**
 * Completion key for console events
 */
#define CONSOLE_COMPLETION_KEY 0xC0701E

/**
 * Maximum line length (including null terminator)
 */
#define CONSOLE_MAX_LINE 4096

/**
 * Detect console type
 * @returns Console type (REAL/PIPE/FILE/NONE)
 */
console_type_t console_detect_type(void);

/**
 * Initialize console worker
 * 
 * Creates worker thread that reads from stdin and enqueues completed lines.
 * Worker posts completion to async_runtime after each line.
 *
 * @param runtime Async runtime for posting completions
 * @param queue Message queue for console lines
 * @param completion_key Completion key (typically CONSOLE_COMPLETION_KEY)
 * @returns Console worker context, or NULL on failure
 */
console_worker_context_t* console_worker_init(async_runtime_t* runtime, async_queue_t* queue, uintptr_t completion_key);

/**
 * Shutdown console worker
 * 
 * Signals worker to stop, waits up to 5 seconds for clean shutdown.
 * 
 * @param ctx Console worker context
 * @param timeout_ms Timeout in milliseconds (0 = no wait, -1 = infinite)
 * @returns true if worker stopped cleanly, false on timeout
 */
bool console_worker_shutdown(console_worker_context_t* ctx, int timeout_ms);

/**
 * Destroy console worker
 * 
 * Frees all resources. Must call console_worker_shutdown() first.
 * 
 * @param ctx Console worker context
 */
void console_worker_destroy(console_worker_context_t* ctx);

/**
 * Get console type string (for logging)
 * @param type Console type
 * @returns Human-readable string
 */
const char* console_type_str(console_type_t type);

#endif /* CONSOLE_WORKER_H */
