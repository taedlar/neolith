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

typedef struct {
  int forward_cache_ttl;
  int reverse_cache_ttl;
  int negative_cache_ttl;
  int stale_refresh_window;
  int forward_quota;
  int reverse_quota;
  int refresh_quota;
} addr_resolver_config_t;

typedef enum {
  RESOLVER_REQ_LOOKUP        = 1, /**< Forward/reverse lookup for resolve() efun */
  RESOLVER_REQ_REVERSE_CACHE = 2, /**< Reverse lookup for query_ip_name() cache refresh */
  RESOLVER_REQ_PEER_REFRESH  = 3  /**< Background reverse refresh with lower priority */
} resolver_request_type_t;

typedef struct {
  int in_flight;
  int in_flight_forward;
  int in_flight_reverse;
  int in_flight_refresh;

  unsigned long admitted;
  unsigned long admitted_forward;
  unsigned long admitted_reverse;
  unsigned long admitted_refresh;

  unsigned long dedup_hit;

  unsigned long rejected_global;
  unsigned long rejected_class;
  unsigned long rejected_queue;
  unsigned long rejected_forward;
  unsigned long rejected_reverse;
  unsigned long rejected_refresh;

  unsigned long dropped_driver_priority;
  unsigned long dropped_forward;
  unsigned long dropped_reverse;
  unsigned long dropped_refresh;

  unsigned long timed_out;
  unsigned long completed;
  unsigned long failed;

  unsigned long fwd_cache_hit;          /**< Forward cache hits (fresh, success) */
  unsigned long fwd_cache_miss;         /**< Forward cache misses (not found or TTL expired) */
  unsigned long fwd_cache_negative_hit; /**< Negative forward cache hits (failed lookup still fresh) */
  unsigned long rev_cache_hit;          /**< Reverse cache hits (fresh) */
  unsigned long rev_cache_miss;         /**< Reverse cache misses (not found or TTL expired) */
} resolver_telemetry_t;

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
 * @brief Populate a resolver config struct with built-in defaults.
 * @param config  Destination config struct.
 */
void addr_resolver_config_init_defaults (addr_resolver_config_t *config);

/**
 * @brief Initialize the resolver worker.
 * @param runtime  Async runtime used to post completion notifications.
 * @param config   Resolver cache/config policy supplied by stem/runtime config.
 * @returns 1 on success, 0 on failure.
 */
int  addr_resolver_init   (struct async_runtime_s *runtime,
                           const addr_resolver_config_t *config);

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
 * @brief Enqueue a background reverse refresh task with lowest admission priority.
 * @param cache_addr  IPv4 address in network byte order.
 * @param ip          Dotted-decimal IP string.
 * @param deadline    Absolute time_t after which the task is abandoned.
 * @returns 1 if enqueued, 0 if the resolver rejects the request.
 */
int addr_resolver_enqueue_refresh (unsigned long cache_addr, const char *ip,
                                   time_t deadline);

/**
 * @brief Dequeue one completed result.
 * @param out  Caller-supplied buffer for the result.
 * @returns 1 if a result was dequeued, 0 if the queue is empty.
 */
int addr_resolver_dequeue_result  (resolver_result_t *out);

/** @brief Install or clear the resolver worker test hook. */
void addr_resolver_set_lookup_test_hook (resolver_lookup_test_hook_t hook);

/** @brief Return the currently active resolver configuration. */
void addr_resolver_get_config (addr_resolver_config_t *out);

/** @brief Record one dedup/coalescing hit from socket-level request joining. */
void addr_resolver_note_dedup_hit (void);

/** @brief Compatibility telemetry snapshot used by socket tests. */
int addr_resolver_get_dns_telemetry_snapshot (int *in_flight,
                                              unsigned long *admitted,
                                              unsigned long *dedup_hit,
                                              unsigned long *timed_out);

/** @brief Fetch full resolver telemetry counters. */
void addr_resolver_get_telemetry (resolver_telemetry_t *out);

/**
 * @brief Lookup a hostname in the forward DNS cache.
 * @param hostname     The hostname string to lookup.
 * @param ip_out       Pointer to receive resolved IPv4 address (host byte order) on cache hit.
 * @returns 1 if found and not expired, 0 otherwise (cache miss or expired).
 */
int addr_resolver_forward_cache_get (const char *hostname, uint32_t *ip_out);

/**
 * @brief Add or update a hostname entry in the forward DNS cache.
 * @param hostname     The hostname key.
 * @param ip_address   The resolved IPv4 address (0 for failed/negative caching).
 * @param success      1 for successful lookup, 0 for failed lookup (negative cache).
 */
void addr_resolver_forward_cache_add (const char *hostname, uint32_t ip_address, int success);

/**
 * @brief Lookup an IP address in the reverse DNS cache.
 * @param addr_in_network_order  IPv4 address in network byte order.
 * @returns Pointer to cached hostname string, or NULL if not cached/expired.
 */
const char *addr_resolver_reverse_cache_get (unsigned long addr_in_network_order);

/**
 * @brief Add or update an IP entry in the reverse DNS cache.
 * @param addr_in_network_order  IPv4 address in network byte order.
 * @param hostname               The hostname to cache.
 */
void addr_resolver_reverse_cache_add (unsigned long addr_in_network_order, const char *hostname);

/**
 * @brief Clear both forward and reverse DNS caches.
 * Called during shutdown/cleanup.
 */
void addr_resolver_cache_reset (void);

#ifdef __cplusplus
}
#endif

#endif /* ADDR_RESOLVER_H */
