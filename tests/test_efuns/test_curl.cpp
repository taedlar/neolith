#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

#ifdef PACKAGE_CURL

#include <atomic>
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

      if (select(FD_SETSIZE, &read_set, nullptr, nullptr, &timeout) <= 0) {
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
    EXPECT_EQ(sp->type, T_ARRAY);
    return sp->u.arr;
  }

  int QueryEventCount(object_t *owner) {
    apply_low("query_event_count", owner, 0);
    EXPECT_EQ(sp->type, T_NUMBER);
    int count = static_cast<int>(sp->u.number);
    pop_stack();
    return count;
  }

  void ClearEvents(object_t *owner) {
    apply_low("clear_events", owner, 0);
    pop_stack();
  }
};

static const char kCallbackOwnerCode[] =
  "mixed *last = ({});\n"
  "mixed *events = ({});\n"
  "void create() { last = ({}); events = ({}); }\n"
  "varargs void curl_done(int ok, string payload, mixed a, mixed b) { last = ({ ok, payload, a, b }); events += ({ copy(last) }); }\n"
  "mixed *query_last() { return last; }\n"
  "int query_event_count() { return sizeof(events); }\n"
  "void clear_events() { last = ({}); events = ({}); }\n";

static const char kObserverCode[] =
  "mixed *events = ({});\n"
  "void create() { events = ({}); }\n"
  "void record(mixed ok, mixed payload) { events += ({ ({ ok, payload }) }); }\n"
  "int query_event_count() { return sizeof(events); }\n";

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
  ASSERT_EQ(sp->type, T_NUMBER);
  EXPECT_EQ(sp->u.number, 1);
  pop_stack();

  bool second_transfer_error = false;
  error_context_t econ;
  save_context(&econ);
  if (setjmp(econ.context)) {
    restore_context(&econ);
    second_transfer_error = true;
  }
  else {
    StartTransfer(owner, "curl_done", 18, "second");
    FAIL() << "perform_to() should reject a second active transfer.";
  }
  pop_context(&econ);
  EXPECT_TRUE(second_transfer_error);

  ASSERT_TRUE(PumpUntil([&]() { return QueryEventCount(owner) == 1; }))
    << "Timed out waiting for curl callback";

  current_object = owner;
  f_in_perform();
  ASSERT_EQ(sp->type, T_NUMBER);
  EXPECT_EQ(sp->u.number, 0);
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
  EXPECT_EQ(first->item[0].type, T_NUMBER);
  EXPECT_EQ(first->item[0].u.number, 1);
  EXPECT_EQ(first->item[1].type, T_STRING);
  EXPECT_STREQ(first->item[1].u.string, "hello from curl");
  EXPECT_EQ(first->item[2].type, T_NUMBER);
  EXPECT_EQ(first->item[2].u.number, 123);
  EXPECT_EQ(first->item[3].type, T_STRING);
  EXPECT_STREQ(first->item[3].u.string, "tail");
  pop_stack();

  ClearEvents(owner);

  StartTransfer(owner, "curl_done", 456, "again");
  ASSERT_TRUE(PumpUntil([&]() { return QueryEventCount(owner) == 1; }))
    << "Timed out waiting for second curl callback";

  array_t *second = QueryLast(owner);
  ASSERT_EQ(second->size, 4);
  EXPECT_EQ(second->item[0].u.number, 1);
  EXPECT_STREQ(second->item[1].u.string, "hello from curl");
  EXPECT_EQ(second->item[2].u.number, 456);
  EXPECT_STREQ(second->item[3].u.string, "again");
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
  EXPECT_EQ(result->item[0].type, T_NUMBER);
  EXPECT_EQ(result->item[0].u.number, 1);
  EXPECT_EQ(result->item[1].type, T_STRING);
  EXPECT_STREQ(result->item[1].u.string, "funptr-body");
  EXPECT_EQ(result->item[2].type, T_NUMBER);
  EXPECT_EQ(result->item[2].u.number, 321);
  EXPECT_EQ(result->item[3].type, T_STRING);
  EXPECT_STREQ(result->item[3].u.string, "fp-tail");
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
  ASSERT_EQ(sp->type, T_NUMBER);
  EXPECT_EQ(sp->u.number, 1);
  pop_stack();

  current_object = owner_b;
  f_in_perform();
  ASSERT_EQ(sp->type, T_NUMBER);
  EXPECT_EQ(sp->u.number, 1);
  pop_stack();

  ASSERT_TRUE(PumpUntil([&]() {
    return QueryEventCount(owner_a) == 1 && QueryEventCount(owner_b) == 1;
  }, 2000)) << "Timed out waiting for concurrent curl callbacks";

  array_t *result_a = QueryLast(owner_a);
  ASSERT_EQ(result_a->size, 4);
  EXPECT_EQ(result_a->item[0].type, T_NUMBER);
  EXPECT_EQ(result_a->item[0].u.number, 1);
  EXPECT_EQ(result_a->item[1].type, T_STRING);
  EXPECT_STREQ(result_a->item[1].u.string, "shared-body");
  EXPECT_EQ(result_a->item[2].type, T_NUMBER);
  EXPECT_EQ(result_a->item[2].u.number, 11);
  EXPECT_EQ(result_a->item[3].type, T_STRING);
  EXPECT_STREQ(result_a->item[3].u.string, "owner-a");
  pop_stack();

  array_t *result_b = QueryLast(owner_b);
  ASSERT_EQ(result_b->size, 4);
  EXPECT_EQ(result_b->item[0].type, T_NUMBER);
  EXPECT_EQ(result_b->item[0].u.number, 1);
  EXPECT_EQ(result_b->item[1].type, T_STRING);
  EXPECT_STREQ(result_b->item[1].u.string, "shared-body");
  EXPECT_EQ(result_b->item[2].type, T_NUMBER);
  EXPECT_EQ(result_b->item[2].u.number, 22);
  EXPECT_EQ(result_b->item[3].type, T_STRING);
  EXPECT_STREQ(result_b->item[3].u.string, "owner-b");
  pop_stack();

  current_object = owner_a;
  f_in_perform();
  ASSERT_EQ(sp->type, T_NUMBER);
  EXPECT_EQ(sp->u.number, 0);
  pop_stack();

  current_object = owner_b;
  f_in_perform();
  ASSERT_EQ(sp->type, T_NUMBER);
  EXPECT_EQ(sp->u.number, 0);
  pop_stack();
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
    bool done = (sp->type == T_NUMBER && sp->u.number == 0);
    pop_stack();
    return done;
  }, 700)) << "Destroyed-owner callback unexpectedly reached observer";
}

} // namespace

#endif /* PACKAGE_CURL */