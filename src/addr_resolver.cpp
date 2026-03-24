/**
 * @file addr_resolver.cpp
 * @brief Async hostname resolver — C++ worker implementation
 *
 * Owns the resolver fallback worker pool, task queue, and result queue.
 * Exposes a thin C API declared in addr_resolver.h.
 *
 * The worker pool calls getaddrinfo() / getnameinfo() without blocking the
 * main LPC event loop.  When results are ready workers post a completion
 * notification on the async runtime so the main thread can drain them.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Wrap C headers that lack extern "C" guards. */
extern "C" {
#include "async/async_queue.h"
#include "async/async_worker.h"
#include "async/async_runtime.h"
#include "outbuf.h"
#include "lpc/object.h"
#include "stralloc.h"
}

#ifdef WINSOCK
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <unistd.h>
#endif

#include <cctype>
#include <cstring>
#include <ctime>
#include <mutex>

#include "addr_resolver.h"

#ifdef HAVE_CARES
#  include <ares.h>
#endif

/* =========================================================================
 * Internal types
 * ========================================================================= */

namespace {

/** Maximum queue depth shared by both the task and result queues. */
static const int AR_QUEUE_SIZE = 256;
static const int AR_GLOBAL_ADMISSION_CAP = 64;

/** Task sent from main thread to the worker. */
typedef struct {
  resolver_request_type_t type;
  int                     request_id;
  char                    query[RESOLVER_STR_MAX];
  unsigned long           cache_addr;
  time_t                  deadline;
} resolver_task_t;

/* Module-level state — accessed only after init, from either main thread
 * (enqueue) or worker thread (dequeue task, enqueue result).
 * The queues themselves are thread-safe.
 */
static async_queue_t   *s_task_queue   = nullptr;
static async_queue_t   *s_result_queue = nullptr;
static async_worker_t  *s_workers[RESOLVER_FALLBACK_WORKER_COUNT] = {};
static async_runtime_t *s_runtime      = nullptr;
static resolver_lookup_test_hook_t s_lookup_test_hook = nullptr;
static addr_resolver_config_t s_config = {};
static resolver_lookup_request_t lookup_request_table[RESOLVER_LOOKUP_REQUEST_CAPACITY];
static resolver_telemetry_t s_telemetry = {};
static std::mutex s_stats_mutex;

enum resolver_admission_class_t {
  RESOLVER_CLASS_FORWARD = 0,
  RESOLVER_CLASS_REVERSE = 1,
  RESOLVER_CLASS_REFRESH = 2
};

static resolver_admission_class_t resolve_admission_class(resolver_request_type_t type)
{
  if (type == RESOLVER_REQ_LOOKUP)
    return RESOLVER_CLASS_FORWARD;
  if (type == RESOLVER_REQ_PEER_REFRESH)
    return RESOLVER_CLASS_REFRESH;
  return RESOLVER_CLASS_REVERSE;
}

static bool is_driver_initiated_lookup_request(int request_id)
{
  return request_id > RESOLVER_LOOKUP_REQUEST_CAPACITY;
}

static void add_class_inflight(resolver_admission_class_t klass, int delta)
{
  if (klass == RESOLVER_CLASS_FORWARD)
    s_telemetry.in_flight_forward += delta;
  else if (klass == RESOLVER_CLASS_REVERSE)
    s_telemetry.in_flight_reverse += delta;
  else
    s_telemetry.in_flight_refresh += delta;

  s_telemetry.in_flight += delta;
  if (s_telemetry.in_flight_forward < 0)
    s_telemetry.in_flight_forward = 0;
  if (s_telemetry.in_flight_reverse < 0)
    s_telemetry.in_flight_reverse = 0;
  if (s_telemetry.in_flight_refresh < 0)
    s_telemetry.in_flight_refresh = 0;
  if (s_telemetry.in_flight < 0)
    s_telemetry.in_flight = 0;
}

static void add_class_admitted(resolver_admission_class_t klass)
{
  s_telemetry.admitted++;
  if (klass == RESOLVER_CLASS_FORWARD)
    s_telemetry.admitted_forward++;
  else if (klass == RESOLVER_CLASS_REVERSE)
    s_telemetry.admitted_reverse++;
  else
    s_telemetry.admitted_refresh++;
}

static void add_class_rejected(resolver_admission_class_t klass)
{
  if (klass == RESOLVER_CLASS_FORWARD)
    s_telemetry.rejected_forward++;
  else if (klass == RESOLVER_CLASS_REVERSE)
    s_telemetry.rejected_reverse++;
  else
    s_telemetry.rejected_refresh++;
}

static bool can_admit_class(resolver_admission_class_t klass)
{
  if (s_telemetry.in_flight >= AR_GLOBAL_ADMISSION_CAP)
    return false;

  int forward_quota = s_config.forward_quota;
  int reverse_quota = s_config.reverse_quota;
  int refresh_quota = s_config.refresh_quota;

  if (klass == RESOLVER_CLASS_REFRESH)
    {
      return s_telemetry.in_flight_refresh < refresh_quota;
    }

  if (klass == RESOLVER_CLASS_REVERSE)
    {
      int borrowed_refresh = refresh_quota - s_telemetry.in_flight_refresh;
      if (borrowed_refresh < 0)
        borrowed_refresh = 0;
      return s_telemetry.in_flight_reverse < (reverse_quota + borrowed_refresh);
    }

  {
    int reserved_reverse = reverse_quota - s_telemetry.in_flight_reverse;
    int reserved_refresh = refresh_quota - s_telemetry.in_flight_refresh;
    if (reserved_reverse < 0)
      reserved_reverse = 0;
    if (reserved_refresh < 0)
      reserved_refresh = 0;
    {
      int effective_forward_quota = AR_GLOBAL_ADMISSION_CAP - reserved_reverse - reserved_refresh;
      if (effective_forward_quota > forward_quota)
        effective_forward_quota = forward_quota;
      return s_telemetry.in_flight_forward < effective_forward_quota;
    }
  }
}

static bool dequeue_oldest_task_for_driver_priority()
{
  resolver_task_t dropped_task;
  size_t dropped_size = 0;

  if (!async_queue_dequeue(s_task_queue, &dropped_task, sizeof(dropped_task), &dropped_size) ||
      dropped_size != sizeof(dropped_task))
    return false;

  resolver_admission_class_t dropped_class = resolve_admission_class(dropped_task.type);
  add_class_inflight(dropped_class, -1);
  s_telemetry.dropped_driver_priority++;
  if (dropped_class == RESOLVER_CLASS_FORWARD)
    s_telemetry.dropped_forward++;
  else if (dropped_class == RESOLVER_CLASS_REVERSE)
    s_telemetry.dropped_reverse++;
  else
    s_telemetry.dropped_refresh++;
  return true;
}

static int enqueue_task_with_admission(const resolver_task_t *task, bool driver_initiated)
{
  if (task == nullptr || s_task_queue == nullptr)
    return 0;

  std::lock_guard<std::mutex> guard(s_stats_mutex);
  resolver_admission_class_t klass = resolve_admission_class(task->type);

  if (!can_admit_class(klass))
    {
      if (s_telemetry.in_flight >= AR_GLOBAL_ADMISSION_CAP)
        s_telemetry.rejected_global++;
      else
        s_telemetry.rejected_class++;
      add_class_rejected(klass);
      return 0;
    }

  if (async_queue_is_full(s_task_queue))
    {
      if (!driver_initiated || !dequeue_oldest_task_for_driver_priority())
        {
          s_telemetry.rejected_queue++;
          add_class_rejected(klass);
          return 0;
        }
    }

  if (!async_queue_enqueue(s_task_queue, task, sizeof(*task)))
    {
      s_telemetry.rejected_queue++;
      add_class_rejected(klass);
      return 0;
    }

  add_class_inflight(klass, 1);
  add_class_admitted(klass);
  return 1;
}

static void record_task_completion(resolver_request_type_t type, int success, int timed_out)
{
  std::lock_guard<std::mutex> guard(s_stats_mutex);
  resolver_admission_class_t klass = resolve_admission_class(type);

  add_class_inflight(klass, -1);

  if (timed_out)
    s_telemetry.timed_out++;
  else if (success)
    s_telemetry.completed++;
  else
    s_telemetry.failed++;
}

static int normalize_cache_ttl(int ttl)
{
  return ttl >= 0 ? ttl : 0;
}

static int normalize_quota(int quota, int fallback)
{
  return quota > 0 ? quota : fallback;
}

static void sleep_for_ms(unsigned int delay_ms)
{
  if (delay_ms == 0)
    return;

#ifdef WINSOCK
  Sleep(delay_ms);
#else
  usleep(delay_ms * 1000);
#endif
}

static void clear_lookup_request_entry(int index)
{
  if (index < 0 || index >= RESOLVER_LOOKUP_REQUEST_CAPACITY)
    return;

  if (lookup_request_table[index].call_back != nullptr)
    free_string(lookup_request_table[index].call_back);

  if (lookup_request_table[index].name != nullptr)
    free_string(lookup_request_table[index].name);

  if (lookup_request_table[index].ob_to_call != nullptr)
    free_object(lookup_request_table[index].ob_to_call,
                "addr_resolver_release_lookup_request: ");

  lookup_request_table[index].name = nullptr;
  lookup_request_table[index].call_back = nullptr;
  lookup_request_table[index].ob_to_call = nullptr;
}

/* =========================================================================
 * Worker thread
 * ========================================================================= */

static void *resolver_worker_main(void *arg)
{
  async_queue_t  *task_queue = static_cast<async_queue_t *>(arg);
  async_worker_t *worker     = async_worker_current();
  resolver_task_t  task;
  resolver_result_t result;
  size_t task_size = 0;

  while (!async_worker_should_stop(worker))
    {
      unsigned int delay_ms = 0;
      const char *effective_query = nullptr;

      if (!async_queue_dequeue(task_queue, &task, sizeof(task), &task_size))
        {
#ifdef WINSOCK
          Sleep(10);
#else
          usleep(10000);
#endif
          continue;
        }

      if (task_size != sizeof(task))
        continue;

      std::memset(&result, 0, sizeof(result));
      result.type       = task.type;
      result.request_id = task.request_id;
      result.cache_addr = task.cache_addr;
      std::strncpy(result.query, task.query, sizeof(result.query) - 1);
      result.query[sizeof(result.query) - 1] = '\0';

      if (s_lookup_test_hook != nullptr)
        s_lookup_test_hook(task.query, &delay_ms, &effective_query);

      if (effective_query == nullptr)
        effective_query = task.query;

      sleep_for_ms(delay_ms);

      if (std::time(nullptr) >= task.deadline)
        {
          result.timed_out = 1;
        }
      else if (task.type == RESOLVER_REQ_LOOKUP)
        {
          if (std::isdigit(static_cast<unsigned char>(task.query[0])))
            {
              /* IP → hostname (reverse lookup for the resolve() efun) */
              struct sockaddr_in addr;
              char host[NI_MAXHOST];

              std::memset(&addr, 0, sizeof(addr));
              addr.sin_family = AF_INET;
                if (inet_pton(AF_INET, effective_query, &addr.sin_addr) == 1 &&
                  getnameinfo(reinterpret_cast<struct sockaddr *>(&addr),
                              sizeof(addr), host, sizeof(host),
                              nullptr, 0, NI_NAMEREQD) == 0)
                {
                  std::strncpy(result.result, host, sizeof(result.result) - 1);
                  result.result[sizeof(result.result) - 1] = '\0';
                  result.success = 1;
                }
            }
          else
            {
              /* hostname → IP (forward lookup for the resolve() efun) */
              struct addrinfo hints;
              struct addrinfo *peers = nullptr;

              std::memset(&hints, 0, sizeof(hints));
              hints.ai_family   = AF_INET;
              hints.ai_socktype = SOCK_STREAM;

              if (getaddrinfo(effective_query, nullptr, &hints, &peers) == 0)
                {
                  for (struct addrinfo *e = peers; e != nullptr; e = e->ai_next)
                    {
                      if (e->ai_family == AF_INET && e->ai_addr != nullptr)
                        {
                          auto *a4 = reinterpret_cast<struct sockaddr_in *>(e->ai_addr);
                          if (inet_ntop(AF_INET, &a4->sin_addr,
                                        result.result,
                                        sizeof(result.result)) != nullptr)
                            result.success = 1;
                          break;
                        }
                    }
                  freeaddrinfo(peers);
                }
            }
        }
      else if (task.type == RESOLVER_REQ_REVERSE_CACHE ||
               task.type == RESOLVER_REQ_PEER_REFRESH)
        {
          /* IP → hostname (reverse lookup for cache refresh requests) */
          struct sockaddr_in addr;
          char host[NI_MAXHOST];

          std::memset(&addr, 0, sizeof(addr));
          addr.sin_family = AF_INET;
            if (inet_pton(AF_INET, effective_query, &addr.sin_addr) == 1 &&
              getnameinfo(reinterpret_cast<struct sockaddr *>(&addr),
                          sizeof(addr), host, sizeof(host),
                          nullptr, 0, NI_NAMEREQD) == 0)
            {
              std::strncpy(result.result, host, sizeof(result.result) - 1);
              result.result[sizeof(result.result) - 1] = '\0';
              result.success = 1;
            }
        }

      /* Post-deadline check: task may have expired during the DNS call. */
      if (!result.timed_out && std::time(nullptr) >= task.deadline)
        {
          result.timed_out = 1;
          result.success   = 0;
        }

      record_task_completion(task.type, result.success, result.timed_out);

      if (s_result_queue != nullptr &&
          async_queue_enqueue(s_result_queue, &result, sizeof(result)))
        {
          if (s_runtime != nullptr)
            async_runtime_post_completion(s_runtime, RESOLVER_COMPLETION_KEY, 1);
        }
    }

  return nullptr;
}

#ifdef HAVE_CARES

/* =========================================================================
 * c-ares backend — used when HAVE_CARES is defined
 * ========================================================================= */

struct CaresPending {
  int  done;
  int  success;
  char result[RESOLVER_STR_MAX];
};

static void
cares_gethostbyname_callback(void *arg, int status, int timeouts,
                             struct hostent *hostent)
{
  (void)timeouts;
  auto *pending = static_cast<CaresPending *>(arg);
  if (status == ARES_SUCCESS && hostent != nullptr &&
      hostent->h_addrtype == AF_INET && hostent->h_addr_list[0] != nullptr)
    {
      char buf[INET_ADDRSTRLEN];
      if (inet_ntop(AF_INET, hostent->h_addr_list[0],
                    buf, sizeof(buf)) != nullptr)
        {
          std::strncpy(pending->result, buf, sizeof(pending->result) - 1);
          pending->result[sizeof(pending->result) - 1] = '\0';
          pending->success = 1;
        }
    }
  pending->done = 1;
}

static void
cares_getnameinfo_callback(void *arg, int status, int timeouts,
                           char *node, char *service)
{
  (void)timeouts;
  (void)service;
  auto *pending = static_cast<CaresPending *>(arg);
  if (status == ARES_SUCCESS && node != nullptr)
    {
      std::strncpy(pending->result, node, sizeof(pending->result) - 1);
      pending->result[sizeof(pending->result) - 1] = '\0';
      pending->success = 1;
    }
  pending->done = 1;
}

/**
 * @brief Drive the c-ares channel event loop until the query completes
 * or the deadline is reached.
 * One task per channel (channel-per-task pattern); each worker creates
 * a fresh channel, runs the query, then destroys it.
 */
static void
run_cares_until_done(ares_channel channel, CaresPending *pending, time_t deadline)
{
  while (!pending->done)
    {
      fd_set readers, writers;
      FD_ZERO(&readers);
      FD_ZERO(&writers);
      int nfds = ares_fds(channel, &readers, &writers);
      if (nfds == 0)
        break; /* no more pending queries */

      time_t now = std::time(nullptr);
      if (now >= deadline)
        break;

      /* Clamp to 5 s slices so the deadline check fires even if ares_timeout
       * returns a longer interval. */
      long secs_left = static_cast<long>(deadline - now);
      struct timeval tv;
      tv.tv_sec  = secs_left > 5 ? 5 : secs_left;
      tv.tv_usec = 0;

      struct timeval cares_tv;
      struct timeval *tvp = ares_timeout(channel, &tv, &cares_tv);

      int ret = select(nfds, &readers, &writers, nullptr, tvp);
      (void)ret;
      ares_process(channel, &readers, &writers);
    }
}

static void *
cares_worker_main(void *arg)
{
  async_queue_t  *task_queue = static_cast<async_queue_t *>(arg);
  async_worker_t *worker     = async_worker_current();
  resolver_task_t   task;
  resolver_result_t result;
  size_t task_size = 0;

  while (!async_worker_should_stop(worker))
    {
      unsigned int delay_ms = 0;
      const char *effective_query = nullptr;

      if (!async_queue_dequeue(task_queue, &task, sizeof(task), &task_size))
        {
#ifdef WINSOCK
          Sleep(10);
#else
          usleep(10000);
#endif
          continue;
        }

      if (task_size != sizeof(task))
        continue;

      std::memset(&result, 0, sizeof(result));
      result.type       = task.type;
      result.request_id = task.request_id;
      result.cache_addr = task.cache_addr;
      std::strncpy(result.query, task.query, sizeof(result.query) - 1);
      result.query[sizeof(result.query) - 1] = '\0';

      if (s_lookup_test_hook != nullptr)
        s_lookup_test_hook(task.query, &delay_ms, &effective_query);

      if (effective_query == nullptr)
        effective_query = task.query;

      sleep_for_ms(delay_ms);

      if (std::time(nullptr) >= task.deadline)
        {
          result.timed_out = 1;
        }
      else
        {
          ares_channel channel = nullptr;
          if (ares_init(&channel) != ARES_SUCCESS)
            {
              /* Treat channel init failure as a transient timeout. */
              result.timed_out = 1;
            }
          else
            {
              CaresPending pending = {};

              if (task.type == RESOLVER_REQ_LOOKUP)
                {
                  if (std::isdigit(
                        static_cast<unsigned char>(task.query[0])))
                    {
                      /* IP → hostname (reverse lookup for resolve() efun) */
                      struct sockaddr_in sa;
                      std::memset(&sa, 0, sizeof(sa));
                      sa.sin_family = AF_INET;
                      if (inet_pton(AF_INET, effective_query,
                                    &sa.sin_addr) == 1)
                        {
                          ares_getnameinfo(
                            channel,
                            reinterpret_cast<struct sockaddr *>(&sa),
                            static_cast<ares_socklen_t>(sizeof(sa)),
                            ARES_NI_LOOKUPHOST,
                            cares_getnameinfo_callback, &pending);
                          run_cares_until_done(channel, &pending,
                                               task.deadline);
                        }
                      else
                        {
                          pending.done = 1; /* malformed IP — log as failure */
                        }
                    }
                  else
                    {
                      /* hostname → IP (forward lookup for resolve() efun) */
                      ares_gethostbyname(channel, effective_query, AF_INET,
                                         cares_gethostbyname_callback,
                                         &pending);
                      run_cares_until_done(channel, &pending,
                                           task.deadline);
                    }
                }
              else if (task.type == RESOLVER_REQ_REVERSE_CACHE ||
                       task.type == RESOLVER_REQ_PEER_REFRESH)
                {
                  /* IP → hostname (cache refresh requests) */
                  struct sockaddr_in sa;
                  std::memset(&sa, 0, sizeof(sa));
                  sa.sin_family = AF_INET;
                  if (inet_pton(AF_INET, effective_query, &sa.sin_addr) == 1)
                    {
                      ares_getnameinfo(
                        channel,
                        reinterpret_cast<struct sockaddr *>(&sa),
                        static_cast<ares_socklen_t>(sizeof(sa)),
                        ARES_NI_LOOKUPHOST,
                        cares_getnameinfo_callback, &pending);
                      run_cares_until_done(channel, &pending,
                                           task.deadline);
                    }
                  else
                    {
                      pending.done = 1; /* malformed IP */
                    }
                }
              else
                {
                  pending.done = 1; /* unknown request type */
                }

              result.timed_out = !pending.done;
              result.success   = pending.success;
              if (pending.success)
                {
                  std::strncpy(result.result, pending.result,
                               sizeof(result.result) - 1);
                  result.result[sizeof(result.result) - 1] = '\0';
                }

              ares_destroy(channel);
            }
        }

      /* Post-deadline guard: the DNS call itself may have exceeded time. */
      if (!result.timed_out && std::time(nullptr) >= task.deadline)
        {
          result.timed_out = 1;
          result.success   = 0;
        }

      record_task_completion(task.type, result.success, result.timed_out);

      if (s_result_queue != nullptr &&
          async_queue_enqueue(s_result_queue, &result, sizeof(result)))
        {
          if (s_runtime != nullptr)
            async_runtime_post_completion(s_runtime, RESOLVER_COMPLETION_KEY, 1);
        }
    }

  return nullptr;
}

#endif /* HAVE_CARES */

} /* anonymous namespace */

/* =========================================================================
 * Public C API
 * ========================================================================= */

extern "C" {

void
addr_resolver_config_init_defaults(addr_resolver_config_t *config)
{
  if (config == nullptr)
    return;

  config->forward_cache_ttl = 300;
  config->reverse_cache_ttl = 900;
  config->negative_cache_ttl = 30;
  config->stale_refresh_window = 30;
  config->forward_quota = 10;
  config->reverse_quota = 4;
  config->refresh_quota = 2;
}

int
addr_resolver_init(struct async_runtime_s *runtime,
                   const addr_resolver_config_t *config)
{
  addr_resolver_config_t effective_config;

  addr_resolver_config_init_defaults(&effective_config);
  if (config != nullptr)
    effective_config = *config;

  effective_config.forward_cache_ttl = normalize_cache_ttl(effective_config.forward_cache_ttl);
  effective_config.reverse_cache_ttl = normalize_cache_ttl(effective_config.reverse_cache_ttl);
  effective_config.negative_cache_ttl = normalize_cache_ttl(effective_config.negative_cache_ttl);
  effective_config.stale_refresh_window = normalize_cache_ttl(effective_config.stale_refresh_window);
  effective_config.forward_quota = normalize_quota(effective_config.forward_quota, 10);
  effective_config.reverse_quota = normalize_quota(effective_config.reverse_quota, 4);
  effective_config.refresh_quota = normalize_quota(effective_config.refresh_quota, 2);

  if (s_task_queue != nullptr)
    {
      s_config = effective_config;
      if (s_runtime == nullptr && runtime != nullptr)
        s_runtime = runtime;
      return 1; /* already initialized */
    }

  s_config = effective_config;
  s_runtime = runtime;

  s_task_queue = async_queue_create(AR_QUEUE_SIZE, sizeof(resolver_task_t),
                                    static_cast<async_queue_flags_t>(0));
  if (s_task_queue == nullptr)
    return 0;

  s_result_queue = async_queue_create(AR_QUEUE_SIZE, sizeof(resolver_result_t),
                                      static_cast<async_queue_flags_t>(0));
  if (s_result_queue == nullptr)
    {
      async_queue_destroy(s_task_queue);
      s_task_queue = nullptr;
      return 0;
    }

#ifdef HAVE_CARES
  ares_library_init(ARES_LIB_INIT_ALL);
#endif

#ifdef HAVE_CARES
  auto *worker_fn = cares_worker_main;
#else
  auto *worker_fn = resolver_worker_main;
#endif

  for (int i = 0; i < RESOLVER_FALLBACK_WORKER_COUNT; i++)
    {
      s_workers[i] = async_worker_create(worker_fn, s_task_queue, 0);
      if (s_workers[i] == nullptr)
        {
          for (int j = 0; j < i; j++)
            {
              async_worker_signal_stop(s_workers[j]);
              async_worker_join(s_workers[j], 2000);
              async_worker_destroy(s_workers[j]);
              s_workers[j] = nullptr;
            }

          async_queue_destroy(s_result_queue);
          async_queue_destroy(s_task_queue);
          s_result_queue = nullptr;
          s_task_queue   = nullptr;
          return 0;
        }
    }

  return 1;
}

void
addr_resolver_deinit(void)
{
  for (int i = 0; i < RESOLVER_LOOKUP_REQUEST_CAPACITY; i++)
    clear_lookup_request_entry(i);

  for (int i = 0; i < RESOLVER_FALLBACK_WORKER_COUNT; i++)
    {
      if (s_workers[i] == nullptr)
        continue;

      async_worker_signal_stop(s_workers[i]);
      async_worker_join(s_workers[i], 2000);
      async_worker_destroy(s_workers[i]);
      s_workers[i] = nullptr;
    }

  if (s_result_queue != nullptr)
    {
      async_queue_destroy(s_result_queue);
      s_result_queue = nullptr;
    }

  if (s_task_queue != nullptr)
    {
      async_queue_destroy(s_task_queue);
      s_task_queue = nullptr;
    }

  s_runtime = nullptr;
  s_lookup_test_hook = nullptr;
  addr_resolver_config_init_defaults(&s_config);
  {
    std::lock_guard<std::mutex> guard(s_stats_mutex);
    std::memset(&s_telemetry, 0, sizeof(s_telemetry));
  }

#ifdef HAVE_CARES
  ares_library_cleanup();
#endif
}

int
addr_resolver_reserve_lookup_request(const char *name, const char *call_back, object_t *ob)
{
  if (name == nullptr || call_back == nullptr || ob == nullptr)
    return 0;

  for (int i = 0; i < RESOLVER_LOOKUP_REQUEST_CAPACITY; i++)
    {
      if (lookup_request_table[i].name != nullptr)
        continue;

      lookup_request_table[i].name = make_shared_string(name);
      lookup_request_table[i].call_back = make_shared_string(call_back);
      lookup_request_table[i].ob_to_call = ob;
      add_ref(ob, "addr_resolver_reserve_lookup_request: ");
      return i + 1;
    }

  return 0;
}

const resolver_lookup_request_t *
addr_resolver_get_lookup_request(int request_id)
{
  int index = request_id - 1;

  if (index < 0 || index >= RESOLVER_LOOKUP_REQUEST_CAPACITY)
    return nullptr;

  if (lookup_request_table[index].name == nullptr)
    return nullptr;

  return &lookup_request_table[index];
}

void
addr_resolver_release_lookup_request(int request_id)
{
  clear_lookup_request_entry(request_id - 1);
}

int
addr_resolver_enqueue_lookup(int request_id, const char *query, time_t deadline)
{
  if (s_task_queue == nullptr || query == nullptr)
    return 0;

  resolver_task_t task;
  std::memset(&task, 0, sizeof(task));
  task.type       = RESOLVER_REQ_LOOKUP;
  task.request_id = request_id;
  task.deadline   = deadline;
  std::strncpy(task.query, query, sizeof(task.query) - 1);
  task.query[sizeof(task.query) - 1] = '\0';

  return enqueue_task_with_admission(&task,
                                     is_driver_initiated_lookup_request(request_id));
}

int
addr_resolver_enqueue_reverse(unsigned long cache_addr, const char *ip, time_t deadline)
{
  if (s_task_queue == nullptr || ip == nullptr)
    return 0;

  resolver_task_t task;
  std::memset(&task, 0, sizeof(task));
  task.type       = RESOLVER_REQ_REVERSE_CACHE;
  task.request_id = 0;
  task.cache_addr = cache_addr;
  task.deadline   = deadline;
  std::strncpy(task.query, ip, sizeof(task.query) - 1);
  task.query[sizeof(task.query) - 1] = '\0';

  return enqueue_task_with_admission(&task, false);
}

int
addr_resolver_enqueue_refresh(unsigned long cache_addr, const char *ip, time_t deadline)
{
  if (s_task_queue == nullptr || ip == nullptr)
    return 0;

  resolver_task_t task;
  std::memset(&task, 0, sizeof(task));
  task.type       = RESOLVER_REQ_PEER_REFRESH;
  task.request_id = 0;
  task.cache_addr = cache_addr;
  task.deadline   = deadline;
  std::strncpy(task.query, ip, sizeof(task.query) - 1);
  task.query[sizeof(task.query) - 1] = '\0';

  return enqueue_task_with_admission(&task, false);
}

int
addr_resolver_dequeue_result(resolver_result_t *out)
{
  if (s_result_queue == nullptr || out == nullptr)
    return 0;

  size_t result_size = 0;
  return (async_queue_dequeue(s_result_queue, out, sizeof(*out), &result_size) &&
          result_size == sizeof(*out)) ? 1 : 0;
}

void
addr_resolver_set_lookup_test_hook(resolver_lookup_test_hook_t hook)
{
  s_lookup_test_hook = hook;
}

void
addr_resolver_get_config(addr_resolver_config_t *out)
{
  if (out == nullptr)
    return;

  *out = s_config;
}

void
addr_resolver_note_dedup_hit(void)
{
  std::lock_guard<std::mutex> guard(s_stats_mutex);
  s_telemetry.dedup_hit++;
}

int
addr_resolver_get_dns_telemetry_snapshot(int *in_flight,
                                         unsigned long *admitted,
                                         unsigned long *dedup_hit,
                                         unsigned long *timed_out)
{
  std::lock_guard<std::mutex> guard(s_stats_mutex);

  if (in_flight != nullptr)
    *in_flight = s_telemetry.in_flight;
  if (admitted != nullptr)
    *admitted = s_telemetry.admitted;
  if (dedup_hit != nullptr)
    *dedup_hit = s_telemetry.dedup_hit;
  if (timed_out != nullptr)
    *timed_out = s_telemetry.timed_out;

  return 1;
}

void
addr_resolver_get_telemetry(resolver_telemetry_t *out)
{
  if (out == nullptr)
    return;

  std::lock_guard<std::mutex> guard(s_stats_mutex);
  *out = s_telemetry;
}

/**
 * @brief Reverse lookup cache: IP address -> hostname with TTL support.
 * Used by query_ip_name() to cache reverse DNS lookups.
 */
#define REVERSE_CACHE_SIZE 200
typedef struct reverse_cache_entry_s {
  unsigned long addr;
  char *name;
  time_t updated_at;
} reverse_cache_entry_t;

static reverse_cache_entry_t s_reverse_cache[REVERSE_CACHE_SIZE];
static int s_reverse_cache_cursor = 0;

/**
 * @brief Forward lookup cache: hostname -> IP address with TTL support.
 * Caches both successful and failed lookups (for negative caching).
 */
#define FORWARD_CACHE_SIZE 200
typedef struct forward_cache_entry_s {
  char *hostname;
  uint32_t ip_address;
  time_t updated_at;
  int success;
} forward_cache_entry_t;

static forward_cache_entry_t s_forward_cache[FORWARD_CACHE_SIZE];
static int s_forward_cache_cursor = 0;

int
addr_resolver_forward_cache_get(const char *hostname, uint32_t *ip_out)
{
  int i;
  time_t now;
  addr_resolver_config_t config;

  if (!hostname || !ip_out)
    return 0;

  now = time(NULL);
  addr_resolver_get_config(&config);

  for (i = 0; i < FORWARD_CACHE_SIZE; i++)
    {
      if (s_forward_cache[i].hostname &&
          strcmp(s_forward_cache[i].hostname, hostname) == 0)
        {
          time_t age = now - s_forward_cache[i].updated_at;
          int active_ttl = s_forward_cache[i].success 
                             ? config.forward_cache_ttl 
                             : config.negative_cache_ttl;

          if (active_ttl > 0 && age >= active_ttl)
            return 0; /* TTL expired */

          if (s_forward_cache[i].success)
            {
              *ip_out = s_forward_cache[i].ip_address;
              s_telemetry.fwd_cache_hit++;
              return 1; /* Cache hit */
            }
          else
            {
              s_telemetry.fwd_cache_negative_hit++;
              return 0; /* Negative cache hit - not found */
            }
        }
    }

  s_telemetry.fwd_cache_miss++;
  return 0; /* Cache miss */
}

void
addr_resolver_forward_cache_add(const char *hostname, uint32_t ip_address, int success)
{
  int i;

  if (!hostname || strlen(hostname) == 0)
    return;

  /* Check if hostname already exists; update in place */
  for (i = 0; i < FORWARD_CACHE_SIZE; i++)
    {
      if (s_forward_cache[i].hostname &&
          strcmp(s_forward_cache[i].hostname, hostname) == 0)
        {
          s_forward_cache[i].ip_address = ip_address;
          s_forward_cache[i].success = success;
          s_forward_cache[i].updated_at = time(NULL);
          return;
        }
    }

  /* Find first empty slot and add */
  i = s_forward_cache_cursor;
  if (s_forward_cache[i].hostname)
    free_string(s_forward_cache[i].hostname);

  s_forward_cache[i].hostname = make_shared_string(hostname);
  s_forward_cache[i].ip_address = ip_address;
  s_forward_cache[i].success = success;
  s_forward_cache[i].updated_at = time(NULL);

  s_forward_cache_cursor = (s_forward_cache_cursor + 1) % FORWARD_CACHE_SIZE;
}

const char *
addr_resolver_reverse_cache_get(unsigned long addr_in_network_order)
{
  int i;
  time_t now;
  addr_resolver_config_t config;

  now = time(NULL);
  addr_resolver_get_config(&config);

  for (i = 0; i < REVERSE_CACHE_SIZE; i++)
    {
      if (s_reverse_cache[i].addr == addr_in_network_order && s_reverse_cache[i].name)
        {
          if (config.reverse_cache_ttl > 0 &&
              s_reverse_cache[i].updated_at > 0 &&
              now >= s_reverse_cache[i].updated_at + config.reverse_cache_ttl)
            {
              return NULL; /* TTL expired */
            }
          s_telemetry.rev_cache_hit++;
          return s_reverse_cache[i].name; /* Cache hit */
        }
    }

  s_telemetry.rev_cache_miss++;
  return NULL; /* Cache miss */
}

void
addr_resolver_reverse_cache_add(unsigned long addr_in_network_order, const char *hostname)
{
  int i;

  if (!hostname || strlen(hostname) == 0 || strcmp(hostname, "0") == 0)
    return;

  /* Check if address already exists; update in place */
  for (i = 0; i < REVERSE_CACHE_SIZE; i++)
    {
      if (s_reverse_cache[i].addr == addr_in_network_order)
        {
          if (s_reverse_cache[i].name)
            free_string(s_reverse_cache[i].name);
          s_reverse_cache[i].name = make_shared_string(hostname);
          s_reverse_cache[i].updated_at = time(NULL);
          return;
        }
    }

  /* Find first empty slot and add */
  i = s_reverse_cache_cursor;
  s_reverse_cache[i].addr = addr_in_network_order;
  if (s_reverse_cache[i].name)
    free_string(s_reverse_cache[i].name);
  s_reverse_cache[i].name = make_shared_string(hostname);
  s_reverse_cache[i].updated_at = time(NULL);

  s_reverse_cache_cursor = (s_reverse_cache_cursor + 1) % REVERSE_CACHE_SIZE;
}

void
addr_resolver_cache_reset(void)
{
  int i;

  /* Reset forward cache */
  for (i = 0; i < FORWARD_CACHE_SIZE; i++)
    {
      if (s_forward_cache[i].hostname)
        {
          free_string(s_forward_cache[i].hostname);
          s_forward_cache[i].hostname = NULL;
        }
      s_forward_cache[i].ip_address = 0;
      s_forward_cache[i].updated_at = 0;
      s_forward_cache[i].success = 0;
    }
  s_forward_cache_cursor = 0;

  /* Reset reverse cache */
  for (i = 0; i < REVERSE_CACHE_SIZE; i++)
    {
      if (s_reverse_cache[i].name)
        {
          free_string(s_reverse_cache[i].name);
          s_reverse_cache[i].name = NULL;
        }
      s_reverse_cache[i].addr = 0;
      s_reverse_cache[i].updated_at = 0;
    }
  s_reverse_cache_cursor = 0;
}

} /* extern "C" */
