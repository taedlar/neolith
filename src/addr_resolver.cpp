/**
 * @file addr_resolver.cpp
 * @brief Async hostname resolver — C++ worker implementation
 *
 * Owns the resolver worker thread, task queue, and result queue.
 * Exposes a thin C API declared in addr_resolver.h.
 *
 * The worker thread calls getaddrinfo() / getnameinfo() without blocking
 * the main LPC event loop.  When results are ready it posts a completion
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

#include "addr_resolver.h"

/* =========================================================================
 * Internal types
 * ========================================================================= */

namespace {

/** Maximum queue depth shared by both the task and result queues. */
static const int AR_QUEUE_SIZE = 256;

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
static async_worker_t  *s_worker       = nullptr;
static async_runtime_t *s_runtime      = nullptr;
static resolver_lookup_request_t lookup_request_table[RESOLVER_LOOKUP_REQUEST_CAPACITY];

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
              if (inet_pton(AF_INET, task.query, &addr.sin_addr) == 1 &&
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

              if (getaddrinfo(task.query, nullptr, &hints, &peers) == 0)
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
      else if (task.type == RESOLVER_REQ_REVERSE_CACHE)
        {
          /* IP → hostname (reverse lookup for query_ip_name() cache refresh) */
          struct sockaddr_in addr;
          char host[NI_MAXHOST];

          std::memset(&addr, 0, sizeof(addr));
          addr.sin_family = AF_INET;
          if (inet_pton(AF_INET, task.query, &addr.sin_addr) == 1 &&
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

      if (s_result_queue != nullptr &&
          async_queue_enqueue(s_result_queue, &result, sizeof(result)))
        {
          async_runtime_post_completion(s_runtime, RESOLVER_COMPLETION_KEY, 1);
        }
    }

  return nullptr;
}

} /* anonymous namespace */

/* =========================================================================
 * Public C API
 * ========================================================================= */

extern "C" {

int
addr_resolver_init(struct async_runtime_s *runtime)
{
  if (s_task_queue != nullptr)
    return 1; /* already initialized */

  s_runtime = runtime;

  s_task_queue = async_queue_create(AR_QUEUE_SIZE, sizeof(resolver_task_t),
                                    ASYNC_QUEUE_DROP_OLDEST);
  if (s_task_queue == nullptr)
    return 0;

  s_result_queue = async_queue_create(AR_QUEUE_SIZE, sizeof(resolver_result_t),
                                      ASYNC_QUEUE_DROP_OLDEST);
  if (s_result_queue == nullptr)
    {
      async_queue_destroy(s_task_queue);
      s_task_queue = nullptr;
      return 0;
    }

  s_worker = async_worker_create(resolver_worker_main, s_task_queue, 0);
  if (s_worker == nullptr)
    {
      async_queue_destroy(s_result_queue);
      async_queue_destroy(s_task_queue);
      s_result_queue = nullptr;
      s_task_queue   = nullptr;
      return 0;
    }

  return 1;
}

void
addr_resolver_deinit(void)
{
  for (int i = 0; i < RESOLVER_LOOKUP_REQUEST_CAPACITY; i++)
    clear_lookup_request_entry(i);

  if (s_worker != nullptr)
    {
      async_worker_signal_stop(s_worker);
      async_worker_join(s_worker, 2000);
      async_worker_destroy(s_worker);
      s_worker = nullptr;
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

  return async_queue_enqueue(s_task_queue, &task, sizeof(task)) ? 1 : 0;
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

  return async_queue_enqueue(s_task_queue, &task, sizeof(task)) ? 1 : 0;
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

} /* extern "C" */
