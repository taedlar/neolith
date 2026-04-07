#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"
#include "lpc/types.hpp"

#ifdef PACKAGE_CURL

#include <atomic>
#include <cstring>
#include <functional>
#include <string>
#include <thread>

extern "C" {
#include "src/apply.h"
#include "src/backend.h"
#include "src/comm.h"
#include "src/error_context.h"
#include "src/simulate.h"
#include "curl/curl_efuns.h"
#include "lpc/buffer.h"
#include "lpc/functional.h"
}

namespace {

class HttpTestServer {
public:
  HttpTestServer(std::string body, int expected_requests = 1, int response_delay_ms = 0)
    : body_(std::move(body)), expected_requests_(expected_requests), response_delay_ms_(response_delay_ms) {
    valid_ = start();
  }

  ~HttpTestServer() {
    stop_ = true;
    if (listener_fd_ != INVALID_SOCKET_FD) {
      SOCKET_CLOSE(listener_fd_);
      listener_fd_ = INVALID_SOCKET_FD;
    }
    if (worker_.joinable()) {
      worker_.join();
    }
  }

  std::string url() const {
    return "http://127.0.0.1:" + std::to_string(port_) + "/";
  }

  bool valid() const {
    return valid_;
  }

  int handled_requests() const {
    return handled_requests_;
  }

private:
  bool start() {
    int one = 1;
    socklen_t len;
    struct sockaddr_in sin;

    listener_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_fd_ == INVALID_SOCKET_FD) {
      return false;
    }

    if (setsockopt(listener_fd_, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one)) == SOCKET_ERROR) {
      SOCKET_CLOSE(listener_fd_);
      listener_fd_ = INVALID_SOCKET_FD;
      return false;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin.sin_port = htons(0);

    if (bind(listener_fd_, reinterpret_cast<struct sockaddr *>(&sin), sizeof(sin)) == SOCKET_ERROR) {
      SOCKET_CLOSE(listener_fd_);
      listener_fd_ = INVALID_SOCKET_FD;
      return false;
    }

    if (listen(listener_fd_, 4) == SOCKET_ERROR) {
      SOCKET_CLOSE(listener_fd_);
      listener_fd_ = INVALID_SOCKET_FD;
      return false;
    }

    len = sizeof(sin);
    if (getsockname(listener_fd_, reinterpret_cast<struct sockaddr *>(&sin), &len) == SOCKET_ERROR) {
      SOCKET_CLOSE(listener_fd_);
      listener_fd_ = INVALID_SOCKET_FD;
      return false;
    }

    port_ = static_cast<int>(ntohs(sin.sin_port));
    worker_ = std::thread([this]() { serve(); });
    return true;
  }

  void serve() {
    while (!stop_ && handled_requests_ < expected_requests_) {
      fd_set read_set;
      struct timeval timeout;
      socket_fd_t client_fd;
      char buffer[2048];

      FD_ZERO(&read_set);
      FD_SET(listener_fd_, &read_set);
      timeout.tv_sec = 0;
      timeout.tv_usec = 100000;

      int nfds;
#ifdef WINSOCK
      nfds = 0;
#else
      nfds = static_cast<int>(listener_fd_) + 1;
#endif

      if (select(nfds, &read_set, nullptr, nullptr, &timeout) <= 0) {
        continue;
      }

      client_fd = accept(listener_fd_, nullptr, nullptr);
      if (client_fd == INVALID_SOCKET_FD) {
        continue;
      }

      (void)recv(client_fd, buffer, sizeof(buffer), 0);
      if (response_delay_ms_ > 0) {
#ifdef WINSOCK
        Sleep(response_delay_ms_);
#else
        usleep(response_delay_ms_ * 1000);
#endif
      }

      std::string response =
        "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " +
        std::to_string(body_.size()) +
        "\r\nConnection: close\r\n\r\n" + body_;
      send(client_fd, response.c_str(), static_cast<int>(response.size()), 0);
      SOCKET_CLOSE(client_fd);
      handled_requests_++;
    }
  }

  std::string body_;
  int expected_requests_ = 0;
  int response_delay_ms_ = 0;
  std::atomic<bool> stop_ = false;
  std::atomic<int> handled_requests_ = 0;
  socket_fd_t listener_fd_ = INVALID_SOCKET_FD;
  int port_ = 0;
  bool valid_ = false;
  std::thread worker_;
};

class CurlEfunsTest : public EfunsTest {
protected:
  void SetUp() override {
    EfunsTest::SetUp();

    if (g_runtime == nullptr) {
      g_runtime = async_runtime_init();
    }
    ASSERT_NE(g_runtime, nullptr) << "Failed to initialize async runtime";

    init_master("/master.c");
    ASSERT_NE(master_ob, nullptr) << "master_ob is null after init_master().";

    init_curl_subsystem();
  }

  void TearDown() override {
    deinit_curl_subsystem();

    if (g_runtime != nullptr) {
      async_runtime_deinit(g_runtime);
      g_runtime = nullptr;
    }

    EfunsTest::TearDown();
  }

  object_t *LoadInlineObject(const char *name, const char *code) {
    object_t *saved_current = current_object;
    object_t *obj;

    current_object = master_ob;
    obj = load_object(name, code);
    current_object = saved_current;
    return obj;
  }

  void ConfigureUrl(object_t *owner, const std::string &url) {
    current_object = owner;
    st_num_arg = 2;
    push_constant_string("url");
    copy_and_push_string(url.c_str());
    f_perform_using();
  }

  void StartTransfer(object_t *owner, const char *callback_name, int carry_number = 0, const char *carry_string = nullptr) {
    current_object = owner;
    st_num_arg = carry_string ? 4 : 3;
    push_constant_string(callback_name);
    push_number(0);
    push_number(carry_number);
    if (carry_string) {
      push_constant_string(carry_string);
    }
    f_perform_to();
  }

  void StartTransferWithFunptr(object_t *owner, funptr_t *callback, int carry_number = 0, const char *carry_string = nullptr) {
    current_object = owner;
    st_num_arg = carry_string ? 4 : 3;
    push_refed_funp(callback);
    push_number(0);
    push_number(carry_number);
    if (carry_string) {
      push_constant_string(carry_string);
    }
    f_perform_to();
  }

  bool PumpUntil(const std::function<bool()> &predicate, int timeout_ms = 3000) {
    int elapsed = 0;
    const int step_ms = 10;

    while (elapsed < timeout_ms) {
      struct timeval timeout;

      if (predicate()) {
        return true;
      }

      timeout.tv_sec = 0;
      timeout.tv_usec = step_ms * 1000;
      (void)do_comm_polling(&timeout);
      process_io();
      elapsed += step_ms;
    }

    return predicate();
  }

  array_t *QueryLast(object_t *owner) {
    apply_low("query_last", owner, 0);
    EXPECT_TRUE(lpc::svalue_view::from(sp).is_array());
    return sp->u.arr;
  }

  int QueryEventCount(object_t *owner) {
    apply_low("query_event_count", owner, 0);
    auto view = lpc::svalue_view::from(sp);
    EXPECT_TRUE(view.is_number());
    int count = static_cast<int>(view.number());
    pop_stack();
    return count;
  }

  void ClearEvents(object_t *owner) {
    apply_low("clear_events", owner, 0);
    pop_stack();
  }

  void ExpectBufferEq(const svalue_t &sv, const char *expected) {
    size_t expected_len = std::strlen(expected);
    ASSERT_EQ(sv.type, T_BUFFER);
    ASSERT_NE(sv.u.buf, nullptr);
    EXPECT_EQ(static_cast<size_t>(sv.u.buf->size), expected_len);
    EXPECT_EQ(std::memcmp(sv.u.buf->item, expected, expected_len), 0);
  }

  void ExpectArrayItemNumber(const array_t *arr, int index, int64_t expected) {
    auto view = lpc::svalue_view::from(&arr->item[index]);
    ASSERT_TRUE(view.is_number());
    EXPECT_EQ(view.number(), expected);
  }

  void ExpectArrayItemString(const array_t *arr, int index, const char *expected) {
    auto view = lpc::svalue_view::from(&arr->item[index]);
    ASSERT_TRUE(view.is_string());
    EXPECT_STREQ(view.c_str(), expected);
  }

  void ExpectTopNumber(int64_t expected) {
    auto view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(view.is_number());
    EXPECT_EQ(view.number(), expected);
  }
};

static const char kCallbackOwnerCode[] =
  "mixed *last = ({});\n"
  "mixed *events = ({});\n"
  "int event_count = 0;\n"
  "void create() { last = ({}); events = ({}); event_count = 0; }\n"
  "varargs void curl_done(int ok, string payload, mixed a, mixed b) { last = ({ ok, payload, a, b }); events += ({ copy(last) }); event_count++; }\n"
  "varargs int try_perform_to(string callback, int flags, mixed a, mixed b) { return catch(perform_to(callback, flags, a, b)) ? 1 : 0; }\n"
  "mixed *query_last() { return last; }\n"
  "int query_event_count() { return event_count; }\n"
  "void clear_events() { last = ({}); events = ({}); event_count = 0; }\n";

static const char kObserverCode[] =
  "mixed *events = ({});\n"
  "int event_count = 0;\n"
  "void create() { events = ({}); event_count = 0; }\n"
  "void record(mixed ok, mixed payload) { events += ({ ({ ok, payload }) }); event_count++; }\n"
  "int query_event_count() { return event_count; }\n";

static const char kDestroyOwnerCode[] =
  "object observer;\n"
  "void set_observer(object ob) { observer = ob; }\n"
  "void curl_done(int ok, string payload) { if (observer) observer->record(ok, payload); }\n";

TEST_F(CurlEfunsTest, PerformToRejectsInvalidCallbackAndFlagTypes) {
  object_t *owner = LoadInlineObject("/tests/efuns/curl_invalid_owner", kCallbackOwnerCode);
  ASSERT_NE(owner, nullptr);

  HttpTestServer server("ignored");
  ASSERT_TRUE(server.valid());
  ConfigureUrl(owner, server.url());

  bool callback_error = false;
  error_context_t econ;
  save_context(&econ);
  if (setjmp(econ.context)) {
    restore_context(&econ);
    callback_error = true;
  }
  else {
    current_object = owner;
    st_num_arg = 2;
    push_number(42);
    push_number(0);
    f_perform_to();
    FAIL() << "perform_to() should reject non-string/non-function callbacks.";
  }
  pop_context(&econ);
  EXPECT_TRUE(callback_error);

  bool flag_error = false;
  save_context(&econ);
  if (setjmp(econ.context)) {
    restore_context(&econ);
    flag_error = true;
  }
  else {
    current_object = owner;
    st_num_arg = 2;
    push_constant_string("curl_done");
    push_constant_string("bad");
    f_perform_to();
    FAIL() << "perform_to() should reject non-number flags.";
  }
  pop_context(&econ);
  EXPECT_TRUE(flag_error);
}

TEST_F(CurlEfunsTest, InPerformAndOneActiveTransferEnforced) {
  object_t *owner = LoadInlineObject("/tests/efuns/curl_active_owner", kCallbackOwnerCode);
  ASSERT_NE(owner, nullptr);

  HttpTestServer server("delayed-body", 1, 300);
  ASSERT_TRUE(server.valid());
  ConfigureUrl(owner, server.url());
  StartTransfer(owner, "curl_done", 17, "pending");

  current_object = owner;
  f_in_perform();
  {
    auto view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(view.is_number());
    EXPECT_EQ(view.number(), 1);
  }
  pop_stack();

  push_constant_string("curl_done");
  push_number(0);
  push_number(18);
  push_constant_string("second");
  apply_low("try_perform_to", owner, 4);
  {
    auto view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(view.is_number());
    EXPECT_EQ(view.number(), 1);
  }
  pop_stack();

  ASSERT_TRUE(PumpUntil([&]() { return QueryEventCount(owner) == 1; }))
    << "Timed out waiting for curl callback";

  current_object = owner;
  f_in_perform();
  {
    auto view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(view.is_number());
    EXPECT_EQ(view.number(), 0);
  }
  pop_stack();
}

TEST_F(CurlEfunsTest, CallbackOrderingAndOptionPersistenceAcrossIdlePeriods) {
  object_t *owner = LoadInlineObject("/tests/efuns/curl_callback_owner", kCallbackOwnerCode);
  ASSERT_NE(owner, nullptr);

  HttpTestServer server("hello from curl", 2);
  ASSERT_TRUE(server.valid());
  ConfigureUrl(owner, server.url());

  StartTransfer(owner, "curl_done", 123, "tail");
  ASSERT_TRUE(PumpUntil([&]() { return QueryEventCount(owner) == 1; }))
    << "Timed out waiting for first curl callback";

  array_t *first = QueryLast(owner);
  ASSERT_EQ(first->size, 4);
  ExpectArrayItemNumber(first, 0, 1);
  ExpectBufferEq(first->item[1], "hello from curl");
  ExpectArrayItemNumber(first, 2, 123);
  ExpectArrayItemString(first, 3, "tail");
  pop_stack();

  ClearEvents(owner);

  StartTransfer(owner, "curl_done", 456, "again");
  ASSERT_TRUE(PumpUntil([&]() { return QueryEventCount(owner) == 1; }))
    << "Timed out waiting for second curl callback";

  array_t *second = QueryLast(owner);
  ASSERT_EQ(second->size, 4);
  ExpectArrayItemNumber(second, 0, 1);
  ExpectBufferEq(second->item[1], "hello from curl");
  ExpectArrayItemNumber(second, 2, 456);
  ExpectArrayItemString(second, 3, "again");
  pop_stack();
}

TEST_F(CurlEfunsTest, FunctionPointerCallbackDispatchesCarryoverArguments) {
  object_t *owner = LoadInlineObject("/tests/efuns/curl_funptr_owner", kCallbackOwnerCode);
  ASSERT_NE(owner, nullptr);

  HttpTestServer server("funptr-body", 1);
  ASSERT_TRUE(server.valid());
  ConfigureUrl(owner, server.url());

  current_object = owner;
  funptr_t *callback = make_lfun_funp_by_name("curl_done", &const0);
  ASSERT_NE(callback, nullptr) << "Failed to create LPC function pointer callback";

  StartTransferWithFunptr(owner, callback, 321, "fp-tail");
  ASSERT_TRUE(PumpUntil([&]() { return QueryEventCount(owner) == 1; }))
    << "Timed out waiting for function pointer curl callback";

  array_t *result = QueryLast(owner);
  ASSERT_EQ(result->size, 4);
  ExpectArrayItemNumber(result, 0, 1);
  ExpectBufferEq(result->item[1], "funptr-body");
  ExpectArrayItemNumber(result, 2, 321);
  ExpectArrayItemString(result, 3, "fp-tail");
  pop_stack();
}

TEST_F(CurlEfunsTest, DistinctObjectsCanTransferConcurrently) {
  object_t *owner_a = LoadInlineObject("/tests/efuns/curl_owner_a", kCallbackOwnerCode);
  object_t *owner_b = LoadInlineObject("/tests/efuns/curl_owner_b", kCallbackOwnerCode);
  ASSERT_NE(owner_a, nullptr);
  ASSERT_NE(owner_b, nullptr);

  HttpTestServer server("shared-body", 2, 150);
  ASSERT_TRUE(server.valid());

  ConfigureUrl(owner_a, server.url());
  ConfigureUrl(owner_b, server.url());

  StartTransfer(owner_a, "curl_done", 11, "owner-a");
  StartTransfer(owner_b, "curl_done", 22, "owner-b");

  current_object = owner_a;
  f_in_perform();
  ExpectTopNumber(1);
  pop_stack();

  current_object = owner_b;
  f_in_perform();
  ExpectTopNumber(1);
  pop_stack();

  ASSERT_TRUE(PumpUntil([&]() {
    return QueryEventCount(owner_a) == 1 && QueryEventCount(owner_b) == 1;
  }, 2000)) << "Timed out waiting for concurrent curl callbacks";

  array_t *result_a = QueryLast(owner_a);
  ASSERT_EQ(result_a->size, 4);
  ExpectArrayItemNumber(result_a, 0, 1);
  ExpectBufferEq(result_a->item[1], "shared-body");
  ExpectArrayItemNumber(result_a, 2, 11);
  ExpectArrayItemString(result_a, 3, "owner-a");
  pop_stack();

  array_t *result_b = QueryLast(owner_b);
  ASSERT_EQ(result_b->size, 4);
  ExpectArrayItemNumber(result_b, 0, 1);
  ExpectBufferEq(result_b->item[1], "shared-body");
  ExpectArrayItemNumber(result_b, 2, 22);
  ExpectArrayItemString(result_b, 3, "owner-b");
  pop_stack();

  current_object = owner_a;
  f_in_perform();
  ExpectTopNumber(0);
  pop_stack();

  current_object = owner_b;
  f_in_perform();
  ExpectTopNumber(0);
  pop_stack();
}

// Verifies that destructing the owner while a transfer is actively in-flight triggers
// the cancel path: close_curl_handles() enqueues CURL_TASK_CANCEL, the worker removes the
// easy handle from curl_multi, and drain_curl_completions() discards the stale completion
// without dispatching a callback.  The server delay (2000ms) ensures the transfer cannot
// complete before the owner is destructed, unlike DestroyedOwnerSkipsCallbackDispatch which
// uses a shorter delay where the transfer may already be done.
TEST_F(CurlEfunsTest, OwnerDestructionCancelsInFlightTransfer) {
  // 2000ms response delay: transfer is still active at the point of destruction.
  HttpTestServer server("cancelled-body", 1, 2000);
  ASSERT_TRUE(server.valid());

  object_t *observer = LoadInlineObject("/tests/efuns/curl_cancel_obs", kObserverCode);
  object_t *owner = LoadInlineObject("/tests/efuns/curl_cancel_owner", kDestroyOwnerCode);
  ASSERT_NE(observer, nullptr);
  ASSERT_NE(owner, nullptr);

  push_object(observer);
  apply_low("set_observer", owner, 1);
  pop_stack();

  ConfigureUrl(owner, server.url());
  StartTransfer(owner, "curl_done");

  // Confirm the transfer is genuinely in-flight before destruction.
  current_object = owner;
  f_in_perform();
  {
    auto view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(view.is_number());
    EXPECT_EQ(view.number(), 1) << "Transfer must be active before destructing the owner";
  }
  pop_stack();

  // Destruct: close_curl_handles() sets owner_ob=nullptr, bumps generation, and posts
  // CURL_TASK_CANCEL.  The callback state is freed here; the transfer may still be running.
  current_object = owner;
  destruct_object(owner);

  // Pump for 800ms: enough for the cancel task to reach the worker, remove the easy handle
  // from curl_multi, post the (now-stale) completion, and for drain_curl_completions() to
  // discard it.  The 2000ms server delay means no real completion arrives in this window.
  bool unexpected_early = PumpUntil([&]() { return QueryEventCount(observer) > 0; }, 800);
  EXPECT_FALSE(unexpected_early) << "No callback expected while cancel drains";

  // Pump past the server delay so any completion that might arrive (e.g. if the network
  // stack completed the transfer despite cancellation) is also processed.  The generation
  // mismatch guard in drain_curl_completions() must discard it without dispatching.
  bool unexpected_delayed = PumpUntil([&]() { return QueryEventCount(observer) > 0; }, 1400);
  EXPECT_FALSE(unexpected_delayed) << "No deferred callback expected after server delay";

  EXPECT_EQ(QueryEventCount(observer), 0) << "Observer must have received zero callbacks";
  // Server may or may not have completed the send (implementation-defined on cancel), but
  // the driver side must never have dispatched a callback regardless.
  EXPECT_EQ(server.handled_requests(), 0) << "Server should not have completed a response before cancel";
}

TEST_F(CurlEfunsTest, DestroyedOwnerSkipsCallbackDispatch) {
  object_t *observer = LoadInlineObject("/tests/efuns/curl_observer", kObserverCode);
  object_t *owner = LoadInlineObject("/tests/efuns/curl_destroy_owner", kDestroyOwnerCode);
  ASSERT_NE(observer, nullptr);
  ASSERT_NE(owner, nullptr);

  HttpTestServer server("destructed", 1, 250);
  ASSERT_TRUE(server.valid());
  ConfigureUrl(owner, server.url());

  push_object(observer);
  apply_low("set_observer", owner, 1);
  pop_stack();

  StartTransfer(owner, "curl_done", 0, nullptr);
  current_object = owner;
  destruct_object(owner);

  ASSERT_TRUE(PumpUntil([&]() {
    apply_low("query_event_count", observer, 0);
    bool done = (lpc::svalue_view::from(sp).is_number() && lpc::svalue_view::from(sp).number() == 0);
    pop_stack();
    return done;
  }, 700)) << "Destroyed-owner callback unexpectedly reached observer";
}

} // namespace

#endif /* PACKAGE_CURL */