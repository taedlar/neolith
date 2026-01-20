/**
 * @file async_worker.h
 * @brief Platform-agnostic worker thread abstraction
 */

#ifndef ASYNC_WORKER_H
#define ASYNC_WORKER_H

#include <stddef.h>
#include <stdbool.h>
#include "port/port_sync.h"

typedef struct async_worker_s async_worker_t;

/**
 * Worker thread procedure signature
 * @param context User-provided context pointer
 * @returns Thread exit code (implementation-defined)
 */
typedef void* (*async_worker_proc_t)(void* context);

/**
 * Worker thread state
 */
typedef enum {
    ASYNC_WORKER_STOPPED,      /**< Thread not running */
    ASYNC_WORKER_RUNNING,      /**< Thread executing */
    ASYNC_WORKER_STOPPING      /**< Shutdown requested */
} async_worker_state_t;

/**
 * Create and start a worker thread
 * 
 * @param proc Thread procedure to execute
 * @param context User context passed to proc
 * @param stack_size Stack size in bytes (0 = platform default)
 * @returns Worker handle, or NULL on failure
 */
async_worker_t* async_worker_create(async_worker_proc_t proc, void* context, size_t stack_size);

/**
 * Destroy worker and free resources
 * Must call async_worker_join() first or worker is leaked
 * 
 * @param worker Worker to destroy
 */
void async_worker_destroy(async_worker_t* worker);

/**
 * Signal worker to stop (non-blocking)
 * Worker should poll async_worker_should_stop() and exit cleanly
 * 
 * @param worker Worker to signal
 */
void async_worker_signal_stop(async_worker_t* worker);

/**
 * Wait for worker thread to exit
 * 
 * @param worker Worker to wait for
 * @param timeout_ms Timeout in milliseconds (-1 = infinite)
 * @returns true if worker exited, false if timeout
 */
bool async_worker_join(async_worker_t* worker, int timeout_ms);

/**
 * Get current worker thread handle (called from worker thread)
 * 
 * @returns Current worker, or NULL if not in worker thread
 */
async_worker_t* async_worker_current(void);

/**
 * Check if worker should stop (called from worker thread)
 * 
 * @param worker Worker to check (use async_worker_current())
 * @returns true if shutdown requested
 */
bool async_worker_should_stop(async_worker_t* worker);

/**
 * Get worker state
 * 
 * @param worker Worker to query
 * @returns Current state
 */
async_worker_state_t async_worker_get_state(const async_worker_t* worker);

/**
 * Get stop event for worker thread
 * For use with platform wait functions (WaitForMultipleObjects/select/etc)
 * 
 * @param worker Worker to query (use async_worker_current())
 * @returns Pointer to stop event, or NULL if invalid
 */
port_event_t* async_worker_get_stop_event(async_worker_t* worker);

#endif /* ASYNC_WORKER_H */
