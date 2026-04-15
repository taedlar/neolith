/**
 * @file curl_efuns.cpp
 * @brief CURL REST API efuns implementation.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef PACKAGE_CURL

#include "src/std.h"
#include "src/apply.h"
#include "src/applies.h"
#include "src/comm.h"
#include "src/error_context.h"
#include "src/interpret.h"
#include "src/simulate.h"
#include "lpc/array.h"
#include "lpc/buffer.h"
#include "lpc/functional.h"
#include "lpc/object.h"
#include "lpc/svalue.h"
#include "lpc/include/origin.h"
#include "lpc/include/runtime_config.h"
#include "rc/rc.h"
#include "async/async_queue.h"
#include "async/async_runtime.h"
#include "async/async_worker.h"

#include <cstdint>
#include <cstring>

#include "curl_efuns.h"

static size_t curl_response_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata);

namespace {

static const int CURL_QUEUE_SIZE = 256;
/* s_curl_handles is allocated once at init and never resized while the worker
 * thread is running.  Keeping the array pointer stable eliminates a data race
 * where the worker holds a derived pointer (including CURLOPT_WRITEDATA) into
 * the array while the main thread could otherwise realloc it. */
static const int CURL_MAX_HANDLE_CAPACITY = 256;
/* With curl_multi_wakeup available, the worker can block longer in
 * curl_multi_poll() and still react immediately when the main thread queues
 * a new transfer/cancel task. On older libcurl, keep the timeout short to
 * avoid extra enqueue-to-start latency. */
#if LIBCURL_VERSION_NUM >= 0x074400
static const int CURL_POLL_TIMEOUT_MS = 1000;
#else
static const int CURL_POLL_TIMEOUT_MS = 50;
#endif

static curl_http_t *s_curl_handles = nullptr;
static int s_num_curl_handles = 0;
static CURLM *s_curl_multi = nullptr;
static async_queue_t *s_task_queue = nullptr;
static async_queue_t *s_result_queue = nullptr;
static async_worker_t *s_worker = nullptr;
static async_runtime_t *s_runtime = nullptr;

static void init_handle_slot(curl_http_t *handle) {
  std::memset(handle, 0, sizeof(*handle));
  handle->state = CURL_STATE_IDLE;
}

static char *dup_bytes(const char *src, size_t len) {
  char *copy;

  copy = reinterpret_cast<char *>(DMALLOC(len + 1, TAG_TEMPORARY, "curl:dup_bytes"));
  if (!copy) {
    return nullptr;
  }

  if (len > 0) {
    std::memcpy(copy, src, len);
  }
  copy[len] = '\0';
  return copy;
}

static char *dup_cstring(const char *src) {
  if (!src) {
    return nullptr;
  }
  return dup_bytes(src, std::strlen(src));
}

static void free_callback_state(curl_http_t *handle) {
  if (handle->callback_is_fp) {
    if (handle->callback.f) {
      free_funp(handle->callback.f);
    }
  }
  else if (handle->callback.s) {
    free_string(to_shared_str(handle->callback.s));
  }

  handle->callback.s = nullptr;
  handle->callback_is_fp = 0;

  if (handle->callback_args) {
    free_array(handle->callback_args);
    handle->callback_args = nullptr;
  }
}

static void free_transfer_buffers(curl_http_t *handle) {
  if (handle->response_buf) {
    FREE(handle->response_buf);
    handle->response_buf = nullptr;
  }
  handle->response_size = 0;
  handle->response_len = 0;

  if (handle->response_headers) {
    curl_slist_free_all(handle->response_headers);
    handle->response_headers = nullptr;
  }

  if (handle->error_msg) {
    FREE(handle->error_msg);
    handle->error_msg = nullptr;
  }

  handle->http_status = 0;
  handle->curl_error = CURLE_OK;
}

static void free_configuration(curl_http_t *handle) {
  if (handle->url) {
    FREE(handle->url);
    handle->url = nullptr;
  }

  if (handle->headers) {
    curl_slist_free_all(handle->headers);
    handle->headers = nullptr;
  }

  if (handle->post_data) {
    FREE(handle->post_data);
    handle->post_data = nullptr;
  }

  handle->post_size = 0;
  handle->timeout_ms = 0;
  handle->follow_location = 0;
}

static void release_handle_slot(curl_http_t *handle) {
  free_callback_state(handle);
  free_transfer_buffers(handle);
  free_configuration(handle);

  if (handle->easy_handle) {
    if (s_curl_multi) {
      curl_multi_remove_handle(s_curl_multi, handle->easy_handle);
    }
    curl_easy_cleanup(handle->easy_handle);
    handle->easy_handle = nullptr;
  }

  init_handle_slot(handle);
}

static void cleanup_detached_handle(curl_http_t *handle) {
  if (handle->owner_ob == nullptr && handle->state != CURL_STATE_TRANSFERRING) {
    release_handle_slot(handle);
  }
}

static void set_error_message(curl_http_t *handle, const char *message) {
  if (handle->error_msg) {
    FREE(handle->error_msg);
    handle->error_msg = nullptr;
  }

  handle->error_msg = dup_cstring(message ? message : "curl transfer failed");
}

static int ensure_handle_capacity(int required_index) {
  int new_capacity;
  curl_http_t *new_handles;

  if (required_index < s_num_curl_handles) {
    return 1;
  }

  new_capacity = CURL_MAX_HANDLE_CAPACITY;

  new_handles = RESIZE(s_curl_handles, new_capacity, curl_http_t, TAG_TEMPORARY, "curl:handle_resize");
  if (!new_handles) {
    return 0;
  }

  s_curl_handles = new_handles;
  while (s_num_curl_handles < new_capacity) {
    init_handle_slot(&s_curl_handles[s_num_curl_handles]);
    s_num_curl_handles++;
  }

  return 1;
}

static int find_handle_index_for_owner(object_t *owner) {
  int index;

  for (index = 0; index < s_num_curl_handles; index++) {
    if (s_curl_handles[index].owner_ob == owner) {
      return index;
    }
  }

  return -1;
}

static int find_handle_index_for_easy(CURL *easy_handle) {
  int index;

  for (index = 0; index < s_num_curl_handles; index++) {
    if (s_curl_handles[index].easy_handle == easy_handle) {
      return index;
    }
  }

  return -1;
}

static int allocate_handle_index(object_t *owner) {
  int index;

  index = find_handle_index_for_owner(owner);
  if (index >= 0) {
    return index;
  }

  for (index = 0; index < s_num_curl_handles; index++) {
    if (s_curl_handles[index].owner_ob == nullptr && s_curl_handles[index].state != CURL_STATE_TRANSFERRING) {
      s_curl_handles[index].owner_ob = owner;
      s_curl_handles[index].state = CURL_STATE_IDLE;
      return index;
    }
  }

  /* All CURL_MAX_HANDLE_CAPACITY slots are allocated at init and never grown;
   * reaching here means every slot is occupied. */
  return -1;
}

static int ensure_easy_handle(curl_http_t *handle) {
  if (handle->easy_handle) {
    return 1;
  }

  handle->easy_handle = curl_easy_init();
  return handle->easy_handle != nullptr;
}

static int is_clear_value(const svalue_t *value) {
  return value->type == T_NUMBER && value->u.number == 0;
}

static void clear_headers(curl_http_t *handle) {
  if (handle->headers) {
    curl_slist_free_all(handle->headers);
    handle->headers = nullptr;
  }
}

static void set_headers_from_array(curl_http_t *handle, array_t *headers) {
  struct curl_slist *new_headers = nullptr;
  int index;

  for (index = 0; index < headers->size; index++) {
    svalue_t *item = &headers->item[index];

    if (item->type != T_STRING) {
      if (new_headers) {
        curl_slist_free_all(new_headers);
      }
      error("perform_using(headers, value) requires a string or string array.\n");
    }

    new_headers = curl_slist_append(new_headers, SVALUE_STRPTR(item));
    if (!new_headers) {
      error("Out of memory while configuring CURL headers.\n");
    }
  }

  clear_headers(handle);
  handle->headers = new_headers;
}

static void configure_option(curl_http_t *handle, const char *option, svalue_t *value) {
  if (!std::strcmp(option, "url")) {
    if (is_clear_value(value)) {
      if (handle->url) {
        FREE(handle->url);
        handle->url = nullptr;
      }
      return;
    }

    if (value->type != T_STRING) {
      error("perform_using(url, value) requires a string.\n");
    }

    if (handle->url) {
      FREE(handle->url);
    }
    handle->url = dup_bytes(SVALUE_STRPTR(value), SVALUE_STRLEN(value));
    if (!handle->url) {
      error("Out of memory while configuring CURL url.\n");
    }
    return;
  }

  if (!std::strcmp(option, "headers")) {
    if (is_clear_value(value)) {
      clear_headers(handle);
      return;
    }

    if (value->type == T_STRING) {
      array_t *single = allocate_empty_array(1);
      assign_svalue_no_free(&single->item[0], value);
      set_headers_from_array(handle, single);
      free_array(single);
      return;
    }

    if (value->type != T_ARRAY) {
      error("perform_using(headers, value) requires a string or string array.\n");
    }

    set_headers_from_array(handle, value->u.arr);
    return;
  }

  if (!std::strcmp(option, "post_data") || !std::strcmp(option, "body")) {
    size_t size;

    if (handle->post_data) {
      FREE(handle->post_data);
      handle->post_data = nullptr;
      handle->post_size = 0;
    }

    if (is_clear_value(value)) {
      return;
    }

    if (value->type == T_STRING) {
      size = SVALUE_STRLEN(value);
      handle->post_data = dup_bytes(SVALUE_STRPTR(value), size);
      handle->post_size = static_cast<int>(size);
    }
    else if (value->type == T_BUFFER) {
      size = value->u.buf->size;
      handle->post_data = dup_bytes(reinterpret_cast<const char *>(value->u.buf->item), size);
      handle->post_size = static_cast<int>(size);
    }
    else {
      error("perform_using(post_data, value) requires a string or buffer.\n");
    }

    if (!handle->post_data) {
      error("Out of memory while configuring CURL post data.\n");
    }
    return;
  }

  if (!std::strcmp(option, "timeout_ms")) {
    if (value->type != T_NUMBER) {
      error("perform_using(timeout_ms, value) requires a number.\n");
    }

    handle->timeout_ms = value->u.number > 0 ? static_cast<long>(value->u.number) : 0L;
    return;
  }

  if (!std::strcmp(option, "follow_location")) {
    if (value->type != T_NUMBER) {
      error("perform_using(follow_location, value) requires a number.\n");
    }

    handle->follow_location = value->u.number != 0;
    return;
  }

  error("Unknown perform_using() option.\n");
}

static int configure_easy_for_transfer(int handle_id) {
  curl_http_t *handle = &s_curl_handles[handle_id];
  CURLcode code;

  if (!handle->url) {
    set_error_message(handle, "perform_to() requires perform_using(\"url\", value) first");
    return 0;
  }

  if (!ensure_easy_handle(handle)) {
    set_error_message(handle, "Failed to create CURL easy handle");
    return 0;
  }

  free_transfer_buffers(handle);
  curl_easy_reset(handle->easy_handle);

  code = curl_easy_setopt(handle->easy_handle, CURLOPT_PRIVATE,
                          reinterpret_cast<void *>(static_cast<uintptr_t>(handle_id + 1)));
  if (code != CURLE_OK) {
    set_error_message(handle, curl_easy_strerror(code));
    return 0;
  }

  code = curl_easy_setopt(handle->easy_handle, CURLOPT_URL, handle->url);
  if (code == CURLE_OK) {
    code = curl_easy_setopt(handle->easy_handle, CURLOPT_WRITEFUNCTION, &curl_response_write_callback);
  }
  if (code == CURLE_OK) {
    code = curl_easy_setopt(handle->easy_handle, CURLOPT_WRITEDATA, handle);
  }
  if (code == CURLE_OK) {
    code = curl_easy_setopt(handle->easy_handle, CURLOPT_NOSIGNAL, 1L);
  }
  if (code == CURLE_OK) {
    code = curl_easy_setopt(handle->easy_handle, CURLOPT_FOLLOWLOCATION,
                            handle->follow_location ? 1L : 0L);
  }
  if (code == CURLE_OK && handle->timeout_ms > 0) {
    code = curl_easy_setopt(handle->easy_handle, CURLOPT_TIMEOUT_MS, handle->timeout_ms);
  }
  if (code == CURLE_OK) {
    code = curl_easy_setopt(handle->easy_handle, CURLOPT_HTTPHEADER, handle->headers);
  }
  if (code == CURLE_OK && handle->post_data) {
    code = curl_easy_setopt(handle->easy_handle, CURLOPT_POST, 1L);
  }
  if (code == CURLE_OK && handle->post_data) {
    code = curl_easy_setopt(handle->easy_handle, CURLOPT_POSTFIELDS, handle->post_data);
  }
  if (code == CURLE_OK && handle->post_data) {
    code = curl_easy_setopt(handle->easy_handle, CURLOPT_POSTFIELDSIZE,
                            static_cast<long>(handle->post_size));
  }
  if (code == CURLE_OK && !handle->post_data) {
    code = curl_easy_setopt(handle->easy_handle, CURLOPT_HTTPGET, 1L);
  }

  if (code != CURLE_OK) {
    set_error_message(handle, curl_easy_strerror(code));
    return 0;
  }

  return 1;
}

static void post_completion_record(uint32_t handle_id, uint32_t generation, int success) {
  curl_completion_t completion;

  if (!s_result_queue) {
    return;
  }

  completion.handle_id = handle_id;
  completion.generation = generation;
  completion.success = success;

  if (!async_queue_enqueue(s_result_queue, &completion, sizeof(completion))) {
    debug_message("CURL completion queue is full; dropping completion for handle %u\n", handle_id);
    return;
  }

  if (s_runtime) {
    async_runtime_post_completion(s_runtime, CURL_COMPLETION_KEY, 1);
  }
}

static void wakeup_curl_worker(void) {
#if LIBCURL_VERSION_NUM >= 0x074400
  if (s_curl_multi) {
    (void)curl_multi_wakeup(s_curl_multi);
  }
#endif
}

static void start_transfer_task(const curl_task_t *task) {
  curl_http_t *handle;
  CURLMcode multi_code;

  if (task->handle_id >= static_cast<uint32_t>(s_num_curl_handles)) {
    return;
  }

  handle = &s_curl_handles[task->handle_id];
  if (handle->state != CURL_STATE_TRANSFERRING || handle->active_generation != task->generation) {
    return;
  }

  if (!configure_easy_for_transfer(static_cast<int>(task->handle_id))) {
    post_completion_record(task->handle_id, handle->active_generation, 0);
    return;
  }

  multi_code = curl_multi_add_handle(s_curl_multi, handle->easy_handle);
  if (multi_code != CURLM_OK) {
    set_error_message(handle, curl_multi_strerror(multi_code));
    post_completion_record(task->handle_id, handle->active_generation, 0);
  }
}

static void drain_multi_messages(void) {
  CURLMsg *message;
  int msg_count;

  while ((message = curl_multi_info_read(s_curl_multi, &msg_count)) != nullptr) {
    int handle_id;
    curl_http_t *handle;

    if (message->msg != CURLMSG_DONE) {
      continue;
    }

    handle_id = find_handle_index_for_easy(message->easy_handle);
    if (handle_id < 0) {
      curl_multi_remove_handle(s_curl_multi, message->easy_handle);
      continue;
    }

    handle = &s_curl_handles[handle_id];
    curl_multi_remove_handle(s_curl_multi, message->easy_handle);
    curl_easy_getinfo(message->easy_handle, CURLINFO_RESPONSE_CODE, &handle->http_status);
    handle->curl_error = message->data.result;

    if (message->data.result != CURLE_OK) {
      set_error_message(handle, curl_easy_strerror(message->data.result));
    }

    post_completion_record(static_cast<uint32_t>(handle_id), handle->active_generation,
                           message->data.result == CURLE_OK ? 1 : 0);
  }
}

static void cancel_transfer_task(const curl_task_t *task) {
  curl_http_t *handle;

  if (task->handle_id >= static_cast<uint32_t>(s_num_curl_handles)) {
    return;
  }

  handle = &s_curl_handles[task->handle_id];
  if (handle->active_generation != task->generation) {
    return;
  }

  if (handle->easy_handle && s_curl_multi) {
    curl_multi_remove_handle(s_curl_multi, handle->easy_handle);
  }

  /* Post a stale completion (generation was bumped on the main thread before
   * enqueueing this task) so drain_curl_completions reclaims the slot without
   * dispatching a callback. */
  post_completion_record(task->handle_id, task->generation, 0);
}

static void *curl_worker_main(void *) {
  async_worker_t *worker = async_worker_current();

  while (!async_worker_should_stop(worker)) {
    curl_task_t task;
    size_t task_size = 0;
    int still_running = 0;
    int processed_task = 0;
    CURLMcode multi_code;

    while (async_queue_dequeue(s_task_queue, &task, sizeof(task), &task_size)) {
      if (task_size == sizeof(task)) {
        if (task.type == CURL_TASK_CANCEL) {
          cancel_transfer_task(&task);
        }
        else {
          start_transfer_task(&task);
        }
        processed_task = 1;
      }
    }

    multi_code = curl_multi_perform(s_curl_multi, &still_running);
    while (multi_code == CURLM_CALL_MULTI_PERFORM) {
      multi_code = curl_multi_perform(s_curl_multi, &still_running);
    }
    drain_multi_messages();

    if (still_running > 0 || !processed_task) {
      int numfds = 0;

      curl_multi_poll(s_curl_multi, nullptr, 0, CURL_POLL_TIMEOUT_MS, &numfds);
      multi_code = curl_multi_perform(s_curl_multi, &still_running);
      while (multi_code == CURLM_CALL_MULTI_PERFORM) {
        multi_code = curl_multi_perform(s_curl_multi, &still_running);
      }
      drain_multi_messages();
    }
  }

  return nullptr;
}

static array_t *copy_callback_args(svalue_t *args, int count) {
  array_t *copied;
  int index;

  if (count <= 0) {
    return nullptr;
  }

  copied = allocate_empty_array(count);
  for (index = 0; index < count; index++) {
    assign_svalue_no_free(&copied->item[index], &args[index]);
  }

  return copied;
}

static void store_callback(curl_http_t *handle, svalue_t *callback_value) {
  free_callback_state(handle);

  if (callback_value->type == T_FUNCTION) {
    handle->callback_is_fp = 1;
    handle->callback.f = callback_value->u.fp;
    handle->callback.f->hdr.ref++;
    return;
  }

  if (callback_value->type != T_STRING) {
    error("perform_to() callback must be a string or function pointer.\n");
  }

  handle->callback_is_fp = 0;
  handle->callback.s = make_shared_string(SVALUE_STRPTR(callback_value), NULL);
}

}  // namespace

static size_t curl_response_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
  curl_http_t *handle = reinterpret_cast<curl_http_t *>(userdata);
  size_t chunk_size = size * nmemb;
  int required;
  char *new_buf;

  if (!handle || chunk_size == 0) {
    return 0;
  }

  required = handle->response_len + static_cast<int>(chunk_size) + 1;
  if (required > handle->response_size) {
    int new_size = handle->response_size > 0 ? handle->response_size : 1024;

    while (new_size < required) {
      new_size *= 2;
    }

    new_buf = reinterpret_cast<char *>(DREALLOC(handle->response_buf, new_size, TAG_TEMPORARY,
                                                "curl:response_resize"));
    if (!new_buf) {
      return 0;
    }

    handle->response_buf = new_buf;
    handle->response_size = new_size;
  }

  std::memcpy(handle->response_buf + handle->response_len, ptr, chunk_size);
  handle->response_len += static_cast<int>(chunk_size);
  handle->response_buf[handle->response_len] = '\0';
  return chunk_size;
}

extern "C" void f_perform_using(void) {
  svalue_t *option_value = sp - 1;
  svalue_t *setting_value = sp;
  const char *option;
  int handle_id;
  curl_http_t *handle;

  if (!s_curl_multi) {
    error("CURL subsystem is not available.\n");
  }

  if (option_value->type != T_STRING) {
    error("perform_using() option must be a string.\n");
  }

  handle_id = allocate_handle_index(current_object);
  if (handle_id < 0) {
    error("Failed to allocate CURL handle state.\n");
  }

  handle = &s_curl_handles[handle_id];
  if (handle->state == CURL_STATE_TRANSFERRING) {
    error("perform_using() cannot modify an active transfer.\n");
  }

  if (!ensure_easy_handle(handle)) {
    error("Failed to create CURL easy handle.\n");
  }

  option = SVALUE_STRPTR(option_value);
  configure_option(handle, option, setting_value);
  handle->state = CURL_STATE_CONFIGURED;
  pop_n_elems(2);
}

extern "C" void f_perform_to(void) {
  int argc = st_num_arg;
  svalue_t *callback_value;
  svalue_t *flag_value;
  int handle_id;
  curl_http_t *handle;
  curl_task_t task;

  if (!s_curl_multi) {
    error("CURL subsystem is not available.\n");
  }

  if (argc < 2) {
    error("perform_to() requires at least a callback and flags argument.\n");
  }

  callback_value = sp - argc + 1;
  flag_value = callback_value + 1;
  if (flag_value->type != T_NUMBER) {
    error("perform_to() flags argument must be a number.\n");
  }

  handle_id = find_handle_index_for_owner(current_object);
  if (handle_id < 0) {
    error("perform_to() requires perform_using() to configure a request first.\n");
  }

  handle = &s_curl_handles[handle_id];
  if (handle->state == CURL_STATE_TRANSFERRING) {
    error("Only one active perform_to() transfer is allowed per object.\n");
  }
  if (!handle->url) {
    error("perform_to() requires perform_using(\"url\", value) first.\n");
  }

  store_callback(handle, callback_value);
  handle->callback_args = copy_callback_args(flag_value + 1, argc - 2);
  handle->owner_ob = current_object;
  handle->state = CURL_STATE_TRANSFERRING;
  handle->generation++;
  handle->active_generation = handle->generation;
  free_transfer_buffers(handle);

  task.type = CURL_TASK_TRANSFER;
  task.handle_id = static_cast<uint32_t>(handle_id);
  task.generation = handle->active_generation;
  if (!async_queue_enqueue(s_task_queue, &task, sizeof(task))) {
    handle->state = CURL_STATE_CONFIGURED;
    free_callback_state(handle);
    error("CURL transfer queue is full.\n");
  }
  wakeup_curl_worker();

  pop_n_elems(argc);
}

extern "C" void f_in_perform(void) {
  int handle_id = find_handle_index_for_owner(current_object);

  if (handle_id >= 0 && s_curl_handles[handle_id].state == CURL_STATE_TRANSFERRING) {
    push_number(1);
    return;
  }

  push_number(0);
}

void init_curl_subsystem(void) {
  if (s_curl_multi) {
    return;
  }

  if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
    debug_message("Warning: failed to initialize libcurl; PACKAGE_CURL disabled.\n");
    return;
  }

  s_runtime = get_async_runtime();
  s_curl_multi = curl_multi_init();
  if (!s_curl_multi) {
    s_runtime = nullptr;
    curl_global_cleanup();
    debug_message("Warning: failed to initialize CURL multi handle; PACKAGE_CURL disabled.\n");
    return;
  }

  if (!ensure_handle_capacity(CURL_MAX_HANDLE_CAPACITY - 1)) {
    s_runtime = nullptr;
    curl_multi_cleanup(s_curl_multi);
    s_curl_multi = nullptr;
    curl_global_cleanup();
    debug_message("Warning: failed to allocate CURL handle state; PACKAGE_CURL disabled.\n");
    return;
  }

  s_task_queue = async_queue_create(CURL_QUEUE_SIZE, sizeof(curl_task_t), static_cast<async_queue_flags_t>(0));
  s_result_queue = async_queue_create(CURL_QUEUE_SIZE, sizeof(curl_completion_t), static_cast<async_queue_flags_t>(0));
  if (!s_task_queue || !s_result_queue) {
    if (s_task_queue) {
      async_queue_destroy(s_task_queue);
      s_task_queue = nullptr;
    }
    if (s_result_queue) {
      async_queue_destroy(s_result_queue);
      s_result_queue = nullptr;
    }
    FREE(s_curl_handles);
    s_curl_handles = nullptr;
    s_num_curl_handles = 0;
    curl_multi_cleanup(s_curl_multi);
    s_curl_multi = nullptr;
    s_runtime = nullptr;
    curl_global_cleanup();
    debug_message("Warning: failed to create CURL queues; PACKAGE_CURL disabled.\n");
    return;
  }

  s_worker = async_worker_create(&curl_worker_main, nullptr, 0);
  if (!s_worker) {
    async_queue_destroy(s_task_queue);
    async_queue_destroy(s_result_queue);
    s_task_queue = nullptr;
    s_result_queue = nullptr;
    FREE(s_curl_handles);
    s_curl_handles = nullptr;
    s_num_curl_handles = 0;
    curl_multi_cleanup(s_curl_multi);
    s_curl_multi = nullptr;
    s_runtime = nullptr;
    curl_global_cleanup();
    debug_message("Warning: failed to start CURL worker; PACKAGE_CURL disabled.\n");
  }
}

void deinit_curl_subsystem(void) {
  int index;

  if (s_worker) {
    async_worker_signal_stop(s_worker);
    async_worker_join(s_worker, -1);
    async_worker_destroy(s_worker);
    s_worker = nullptr;
  }

  for (index = 0; index < s_num_curl_handles; index++) {
    release_handle_slot(&s_curl_handles[index]);
  }

  if (s_curl_handles) {
    FREE(s_curl_handles);
    s_curl_handles = nullptr;
  }
  s_num_curl_handles = 0;

  if (s_task_queue) {
    async_queue_destroy(s_task_queue);
    s_task_queue = nullptr;
  }
  if (s_result_queue) {
    async_queue_destroy(s_result_queue);
    s_result_queue = nullptr;
  }

  if (s_curl_multi) {
    curl_multi_cleanup(s_curl_multi);
    s_curl_multi = nullptr;
  }

  curl_global_cleanup();
  s_runtime = nullptr;
}

void close_curl_handles(object_t *ob) {
  int index;

  for (index = 0; index < s_num_curl_handles; index++) {
    curl_http_t *handle = &s_curl_handles[index];

    if (handle->owner_ob != ob) {
      continue;
    }

    free_callback_state(handle);
    if (handle->state == CURL_STATE_TRANSFERRING) {
      curl_task_t cancel_task;
      uint32_t active_gen = handle->active_generation;

      handle->owner_ob = nullptr;
      handle->generation++;

      cancel_task.type = CURL_TASK_CANCEL;
      cancel_task.handle_id = static_cast<uint32_t>(index);
      cancel_task.generation = active_gen;
      if (async_queue_enqueue(s_task_queue, &cancel_task, sizeof(cancel_task))) {
        wakeup_curl_worker();
      }
      // failure to enqueue a cancel task is not critical since the generation
      // bump above ensures the completed transfer will be treated as stale and
      // cleaned up without a callback.
      continue;
    }

    release_handle_slot(handle);
  }
}

void drain_curl_completions(void) {
  curl_completion_t completion;
  size_t completion_size = 0;

  while (s_result_queue && async_queue_dequeue(s_result_queue, &completion, sizeof(completion), &completion_size)) {
    curl_http_t *handle;
    object_t *owner;
    int arg_count;

    if (completion_size != sizeof(completion) || completion.handle_id >= static_cast<uint32_t>(s_num_curl_handles)) {
      continue;
    }

    handle = &s_curl_handles[completion.handle_id];
    if (completion.generation != handle->generation || handle->state != CURL_STATE_TRANSFERRING) {
      if (handle->owner_ob == nullptr) {
        handle->state = CURL_STATE_IDLE;
      }
      cleanup_detached_handle(handle);
      continue;
    }

    owner = handle->owner_ob;
    if (!owner || (owner->flags & O_DESTRUCTED)) {
      handle->owner_ob = nullptr;
      handle->state = CURL_STATE_IDLE;
      cleanup_detached_handle(handle);
      continue;
    }

    arg_count = 2 + (handle->callback_args ? handle->callback_args->size : 0);

    /* Pre-check response size before touching the stack: allocate_buffer()
     * raises a runtime error when the size exceeds max_buffer_size, which
     * would skip cleanup and leave the handle in TRANSFERRING state forever. */
    int effective_success = completion.success;
    if (effective_success && handle->response_len > CONFIG_INT(__MAX_BUFFER_SIZE__)) {
      effective_success = 0;
    }

    push_number(effective_success);
    if (effective_success) {
      if (handle->response_buf && handle->response_len > 0) {
        buffer_t *buf = allocate_buffer(handle->response_len);
        if (buf) {
          memcpy(buf->item, handle->response_buf, handle->response_len);
        }
        push_refed_buffer(buf);
      }
      else {
        /* Empty response body: push an empty buffer. */
        buffer_t *buf = allocate_buffer(0);
        push_refed_buffer(buf);
      }
    }
    else {
      const char *err;
      if (completion.success && !effective_success) {
        err = "response body exceeds max_buffer_size";
      }
      else {
        err = handle->error_msg ? handle->error_msg : curl_easy_strerror(handle->curl_error);
      }
      copy_and_push_string(err);
    }
    if (handle->callback_args) {
      push_some_svalues(handle->callback_args->item, handle->callback_args->size);
    }

    if (handle->callback_is_fp) {
      safe_call_function_pointer(handle->callback.f, arg_count);
    }
    else if (handle->callback.s) {
      if (handle->callback.s[0] == APPLY___INIT_SPECIAL_CHAR) {
        error("Illegal function name.\n");
      }
      safe_apply(handle->callback.s, owner, arg_count, ORIGIN_DRIVER);
    }

    free_callback_state(handle);
    free_transfer_buffers(handle);
    handle->state = CURL_STATE_CONFIGURED;
    cleanup_detached_handle(handle);
  }
}

#endif /* PACKAGE_CURL */