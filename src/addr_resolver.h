/**
 * @file addr_resolver.h
 * @brief Async hostname resolver worker — public C API
 *
 * Owns a small background worker pool that calls getaddrinfo() /
 * getnameinfo() for forward and reverse DNS lookups without blocking the
 * main LPC event loop.  When results are ready a worker posts a
 * completion notification via the async runtime; the main thread then
 * calls addr_resolver_dequeue_result() inside its event dispatcher.
 *
 * All public functions must be called from the main (backend) thread only.
 */

#ifndef ADDR_RESOLVER_H
#define ADDR_RESOLVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

typedef struct object_s object_t;

/** Completion key posted to the async runtime when results are ready. */
#define RESOLVER_COMPLETION_KEY  0x52455300u

/** Seconds before a pending resolver task expires. */
#define RESOLVER_TIMEOUT_SECONDS 30

/** Maximum length of a hostname or IP string, including the NUL terminator. */
#define RESOLVER_STR_MAX         128

/** Maximum number of outstanding resolve() callbacks awaiting completion. */
#define RESOLVER_LOOKUP_REQUEST_CAPACITY 200

/**
 * @brief Fixed fallback worker-pool size used when c-ares is unavailable.
 */
#define RESOLVER_FALLBACK_WORKER_COUNT 2

typedef enum {
  RESOLVER_REQ_LOOKUP        = 1, /**< Forward/reverse lookup for resolve() efun */
  RESOLVER_REQ_REVERSE_CACHE = 2  /**< Reverse lookup for query_ip_name() cache refresh */
} resolver_request_type_t;

/** Result produced by the resolver worker for one task. */
typedef struct {
  resolver_request_type_t type;
  int                     request_id;
  int                     timed_out;
  int                     success;
  char                    query[RESOLVER_STR_MAX];
  char                    result[RESOLVER_STR_MAX];
  unsigned long           cache_addr;
} resolver_result_t;

/** Lookup callback bookkeeping stored until the worker posts a completion. */
typedef struct {
  char     *name;
  char     *call_back;
  object_t *ob_to_call;
} resolver_lookup_request_t;

/**
 * @brief Test-only hook to inject delay or query rewriting in resolver workers.
 */
typedef void (*resolver_lookup_test_hook_t)(const char *original_query,
                                            unsigned int *delay_ms_out,
                                            const char **effective_query_out);

/**
 * @brief Initialize the resolver worker.
 * @param runtime  Async runtime used to post completion notifications.
 * @returns 1 on success, 0 on failure.
 */
int  addr_resolver_init   (struct async_runtime_s *runtime);

/** @brief Stop the resolver worker and free all resources. */
void addr_resolver_deinit (void);

/**
 * @brief Reserve one pending resolve() callback slot.
 * @param name       Original query string.
 * @param call_back  LPC callback function name.
 * @param ob         Object that will receive the callback.
 * @returns Request id (> 0) on success, 0 if no slot is available.
 */
int addr_resolver_reserve_lookup_request (const char *name,
                                          const char *call_back,
                                          object_t *ob);

/**
 * @brief Fetch a pending resolve() request by request id.
 * @param request_id  Request id returned by addr_resolver_reserve_lookup_request().
 * @returns Pointer to the request entry, or NULL if no such request exists.
 */
const resolver_lookup_request_t *addr_resolver_get_lookup_request (int request_id);

/**
 * @brief Release a pending resolve() request and its retained references.
 * @param request_id  Request id returned by addr_resolver_reserve_lookup_request().
 */
void addr_resolver_release_lookup_request (int request_id);

/**
 * @brief Enqueue a forward/reverse lookup task for the resolve() efun.
 * @param request_id  Request id returned by addr_resolver_reserve_lookup_request().
 * @param query       Hostname or dotted-decimal IP string.
 * @param deadline    Absolute time_t after which the task is abandoned.
 * @returns 1 if enqueued, 0 if the resolver is unavailable or queue is full.
 */
int addr_resolver_enqueue_lookup  (int request_id, const char *query,
                                   time_t deadline);

/**
 * @brief Enqueue a reverse-cache lookup to refresh the query_ip_name() cache.
 * @param cache_addr  IPv4 address in network byte order.
 * @param ip          Dotted-decimal IP string.
 * @param deadline    Absolute time_t after which the task is abandoned.
 * @returns 1 if enqueued, 0 if the resolver is unavailable or queue is full.
 */
int addr_resolver_enqueue_reverse (unsigned long cache_addr, const char *ip,
                                   time_t deadline);

/**
 * @brief Dequeue one completed result.
 * @param out  Caller-supplied buffer for the result.
 * @returns 1 if a result was dequeued, 0 if the queue is empty.
 */
int addr_resolver_dequeue_result  (resolver_result_t *out);

/** @brief Install or clear the resolver worker test hook. */
void addr_resolver_set_lookup_test_hook (resolver_lookup_test_hook_t hook);

#ifdef __cplusplus
}
#endif

#endif /* ADDR_RESOLVER_H */
