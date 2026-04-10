// lib/curl/curl_efuns.h
/**
 * @file curl_efuns.h
 * @brief CURL REST API efuns for non-blocking HTTP requests.
 *
 * Provides perform_using(), perform_to(), and in_perform() efuns when PACKAGE_CURL is enabled.
 */

#ifndef HAVE_CURL_EFUNS_H
#define HAVE_CURL_EFUNS_H

#ifdef PACKAGE_CURL

#include <curl/curl.h>

#include "lpc/object.h"
#include "lpc/array.h"
#include "lpc/functional.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CURL transfer state machine.
 */
typedef enum {
    CURL_STATE_IDLE = 0,        /* No handle or not active */
    CURL_STATE_CONFIGURED = 1,  /* Handle configured, waiting for perform_to */
    CURL_STATE_TRANSFERRING = 2, /* Transfer in progress on worker thread */
    CURL_STATE_COMPLETED = 3,   /* Transfer done, callbacks ready to dispatch */
} curl_state_t;

/**
 * @brief Per-object CURL handle pool entry.
 *
 * One entry per LPC object that uses CURL. The pool is indexed by object address
 * for fast lookup during completion and destruction cleanup.
 */
typedef struct {
    object_t *owner_ob;              /* LPC object owning this handle; NULL if free */
    CURL *easy_handle;               /* Lazy-allocated curl easy handle; NULL if not yet created */
    
    curl_state_t state;              /* Current transfer state */
    uint32_t generation;             /* Incremented on realloc/cancel to invalidate stale completions */
    uint32_t active_generation;      /* Stable token for the currently submitted transfer */
    
    /* Configuration (persists across idle periods) */
    char *url;                       /* Current target URL (null-terminated) */
    struct curl_slist *headers;      /* Custom HTTP headers for this transfer */
    char *post_data;                 /* Request body for POST (null-terminated or size-bounded) */
    int post_size;                   /* Size of post_data in bytes (for binary safety) */
    long timeout_ms;                 /* Transfer timeout in milliseconds */
    int follow_location;             /* 1 to follow redirects, 0 to reject */
    
    /* Callback (stored when perform_to is called) */
    string_or_func_t callback;       /* String function name or funptr_t* to invoke on completion */
    int callback_is_fp;              /* 1 if callback.f is a funptr_t*, 0 if callback.s is a string */
    array_t *callback_args;          /* Carryover arguments for callback (like input_to convention) */
    
    /* Response/Error buffers */
    char *response_buf;              /* Accumulated response body from curl write callback */
    int response_size;               /* Size of response_buf allocated */
    int response_len;                /* Actual bytes written to response_buf */
    int http_status;                 /* HTTP status code from curl_easy_getinfo */
    struct curl_slist *response_headers; /* Response headers (optional, for future use) */
    
    /* Error tracking */
    CURLcode curl_error;             /* Last curl error code (from curl_easy_perform) */
    char *error_msg;                 /* Driver-generated error message if transfer fails */
} curl_http_t;

/**
 * @brief Task type for worker thread dispatch.
 */
typedef enum {
    CURL_TASK_TRANSFER = 0,  /* Start a new transfer via curl_multi_add_handle */
    CURL_TASK_CANCEL = 1,    /* Remove an in-flight transfer from curl_multi */
} curl_task_type_t;

/**
 * @brief Task enqueued to worker thread for curl_multi_perform.
 */
typedef struct {
    uint32_t type;                   /* curl_task_type_t: TRANSFER or CANCEL */
    uint32_t handle_id;              /* Index into curl_handles[] pool */
    uint32_t generation;             /* Generation ID from handle (for stale check) */
} curl_task_t;

/**
 * @brief Result enqueued from worker thread when transfer completes.
 */
typedef struct {
    uint32_t handle_id;              /* Index into curl_handles[] pool */
    uint32_t generation;             /* Generation ID (must match handle's for validity) */
    int success;                     /* 1 if completed successfully, 0 on error */
} curl_completion_t;

/**
 * @brief perform_using() efun: Configure CURL easy handle options for current object.
 * 
 * Sets up request configuration (method, headers, body, etc.) on a per-object easy handle.
 * The handle is created lazily on first call to perform_using and reused for subsequent calls.
 * Configuration can only be changed when no transfer is active.
 *
 * @param opt Option name (mixed: string or constant)
 * @param val Option value (mixed: depends on option)
 */
void f_perform_using(void);

/**
 * @brief perform_to() efun: Initiate non-blocking CURL transfer with callback.
 * 
 * Submits a configured HTTP request and registers a callback to be invoked when complete.
 * Only one active transfer per object is allowed; additional calls to perform_to will fail
 * if a transfer is already in progress.
 *
 * Mirrors input_to() callback convention:
 * - Callback is invoked with (success_flag, body_or_error) plus any carryover arguments.
 * - success_flag: int (1 for success, 0 for failure)
 * - body_or_error: string (response body on success, error message on failure)
 *
 * @param fun Callback function name (string) or function pointer
 * @param flag Flags (reserved for future use; currently unused)
 * @param ... Carryover arguments passed to callback after standard args
 */
void f_perform_to(void);

/**
 * @brief in_perform() efun: Query active transfer status for current object.
 * 
 * Returns non-zero (true) if the current object has an active CURL transfer in progress,
 * zero (false) otherwise. This is a simple status query with no side effects.
 *
 * @return int Non-zero if transfer active, zero if idle.
 */
void f_in_perform(void);

/**
 * @brief CURL subsystem initialization.
 * 
 * Called once during driver startup to initialize the CURL worker thread,
 * event loop integration, and per-object handle pool.
 */
void init_curl_subsystem(void);

/**
 * @brief CURL subsystem cleanup.
 * 
 * Called during driver shutdown to gracefully stop the worker thread,
 * free all CURL handles, drain queues, and release resources.
 */
void deinit_curl_subsystem(void);

/**
 * @brief Cancel and clean up CURL handles for a object.
 *
 * For idle or configured handles, resources are freed immediately.
 * For in-flight transfers a CURL_TASK_CANCEL task is enqueued to the worker,
 * which removes the easy handle from curl_multi and posts a stale completion.
 * drain_curl_completions() then reclaims the slot without dispatching a callback.
 * 
 * @param ob LPC object whose CURL handles should be closed (e.g. on destruction)
 */
void close_curl_handles(object_t *ob);

/**
 * @brief Drain completed CURL transfers and dispatch LPC callbacks.
 * 
 * Called from process_io() on the main thread when CURL_COMPLETION_KEY is received.
 * Dequeues all pending completions, validates generations, and invokes LPC callbacks.
 */
void drain_curl_completions(void);

/**
 * @brief Completion key identifier for CURL worker completions.
 * Used by async_runtime_post_completion() and process_io() dispatch.
 */
#define CURL_COMPLETION_KEY 0x43554C00u

#ifdef __cplusplus
}
#endif

#endif /* PACKAGE_CURL */

#endif /* HAVE_CURL_EFUNS_H */
