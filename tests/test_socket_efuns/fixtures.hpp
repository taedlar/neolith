#pragma once

#include <gtest/gtest.h>
#include <filesystem>
#include <queue>
#include <memory>

extern "C" {
  #include "std.h"
  #include "rc.h"
  #include "lpc/compiler.h"
  #include "lpc/array.h"
  #include "lpc/program.h"
  #include "lpc/svalue.h"
  #include "lpc/types.h"
  #include "lpc/object.h"
  #include "lpc/functional.h"
  #include "socket/socket_efuns.h"
  #include "lpc/include/socket_err.h"
  #include "port/socket_comm.h"
  #include "interpret.h"
}

using namespace testing;

#ifdef WINSOCK
class WinsockEnvironment : public Environment {
public:
  void SetUp() override {
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
      throw std::runtime_error("WSAStartup failed");
    }
  }

  void TearDown() override {
    WSACleanup();
  }
};
#endif

/**
 * @brief Tracks callback invocations for behavioral verification.
 *
 * Records the sequence of callbacks (read, write, close) with their parameters
 * for assertion against the test matrix.
 */
struct CallbackRecord {
  enum CallbackType { CB_READ, CB_WRITE, CB_CLOSE } type;
  int socket_fd;
  int error_code; // for read/write failures
  std::string data; // for read callbacks
  
  CallbackRecord(CallbackType t, int fd, int ec = 0, const std::string& d = "")
    : type(t), socket_fd(fd), error_code(ec), data(d) {}
};

/**
 * @brief Behavior compatibility test fixture for socket efun tests.
 *
 * Provides full LPC runtime initialization, socket helper methods, and
 * callback tracking infrastructure. Each test maps directly to SOCK_BHV_XXX
 * IDs in docs/plan/socket-operation-engine.md.
 *
 * Initializes and deinitializes the LPC runtime per test in SetUp/TearDown.
 * Repeated cycles work correctly in pedantic mode. Per-test setup resets
 * the callback queue and reinitializes the LPC subsystems.
 */
class SocketEfunsBehaviorTest : public Test {
protected:
  // Initialized once per suite; persists across all tests.
  inline static std::filesystem::path s_previous_cwd;

  std::queue<CallbackRecord> callback_records; // ordered callback history

  void SetUp() override {
    namespace fs = std::filesystem;
    // Initialize logging and locale
    debug_set_log_with_date(0);
    setlocale(LC_ALL, PLATFORM_UTF8_LOCALE);

    // Initialize core driver
    fs::path config_dir = fs::current_path();
    if (!fs::exists(config_dir / "m3.conf"))
      config_dir = fs::current_path().parent_path();
    init_stem(3, (unsigned long)-1, (config_dir / "m3.conf").string().c_str());
    init_config(MAIN_OPTION(config_file));
    debug_message("[ SETUP    ] CTEST_FULL_OUTPUT");

    // Verify mudlib path and change to it
    ASSERT_TRUE(CONFIG_STR(__MUD_LIB_DIR__)) << "Mudlib directory not configured";
    auto mudlib_path = fs::path(CONFIG_STR(__MUD_LIB_DIR__));
    if (mudlib_path.is_relative()) {
      mudlib_path = config_dir / mudlib_path;
    }
    ASSERT_TRUE(fs::exists(mudlib_path))
      << "Mudlib directory does not exist: " << mudlib_path;
    s_previous_cwd = fs::current_path();
    fs::current_path(mudlib_path);

    // Initialize LPC string and compiler subsystems
    init_strings(8192, 1000000);
    init_lpc_compiler(CONFIG_INT(__MAX_LOCAL_VARIABLES__),
                      CONFIG_STR(__INCLUDE_DIRS__));

    // Initialize simulate and eval cost
    setup_simulate();
    eval_cost = CONFIG_INT(__MAX_EVAL_COST__);

    // Initialize master object for apply dispatch
    init_master("/master.c");

    // Clear callback queue before each test.
    ClearCallbacks();
  }

  void TearDown() override {
    // Clean up runtime in reverse setup order.
    if (master_ob) {
      close_referencing_sockets(master_ob);
    }
    tear_down_simulate();
    deinit_lpc_compiler();
    deinit_strings();
    deinit_config();

    // Restore working directory
    namespace fs = std::filesystem;
    if (!s_previous_cwd.empty())
      fs::current_path(s_previous_cwd);
  }

  /**
   * @brief Clear the callback record queue for a fresh test.
   */
  void ClearCallbacks() {
    while (!callback_records.empty()) {
      callback_records.pop();
    }
  }

  /**
   * @brief Verify callback record matches expected type and socket fd.
   *
   * Used to assert callback execution order and parameters.
   */
  bool VerifyCallbackType(CallbackRecord::CallbackType expected_type, int expected_fd) {
    if (callback_records.empty()) return false;
    const auto& rec = callback_records.front();
    return rec.type == expected_type && rec.socket_fd == expected_fd;
  }

  /**
   * @brief Pop and consume the next callback record.
   */
  CallbackRecord PopCallback() {
    EXPECT_FALSE(callback_records.empty()) << "Unexpected callback dequeue on empty queue";
    auto rec = callback_records.front();
    callback_records.pop();
    return rec;
  }

  /**
   * @brief Verify no callbacks are pending.
   */
  void ExpectNoCallbacks() {
    EXPECT_TRUE(callback_records.empty()) 
      << "Expected no callbacks but found " << callback_records.size() << " pending";
  }

  object_t* LoadCallbackOwner(const char* object_name = "test_socket_callback_owner.c") {
    static const char callback_owner_code[] =
      "mixed *events = ({});\n"
      "void create() { events = ({}); }\n"
      "void clear_events() { events = ({}); }\n"
      "varargs void read_callback(int fd, mixed payload, mixed extra) { events += ({ ({ \"read\", fd, payload, extra }) }); }\n"
      "varargs void write_callback(int fd, mixed payload, mixed extra) { events += ({ ({ \"write\", fd, payload, extra }) }); }\n"
      "varargs void close_callback(int fd, mixed payload, mixed extra) { events += ({ ({ \"close\", fd, payload, extra }) }); }\n"
      "mixed *query_events() { return events; }\n";

    ScopedObjectContext ctx(this, master_ob);
    object_t* owner = load_object(object_name, callback_owner_code);
    EXPECT_NE(owner, nullptr) << "Failed to load LPC callback owner object";
    return owner;
  }

  void CaptureCallbacksFromOwner(object_t* owner) {
    int index;
    int fio;
    int vio;
    int event_index;
    int runtime_index;
    int saved_vio;
    object_t* saved_current_object;
    program_t* found_prog;
    svalue_t ret;

    ASSERT_NE(owner, nullptr) << "Callback owner object is null";

    ClearCallbacks();

    found_prog = find_function(owner->prog, findstring("query_events"), &index, &fio, &vio);
    ASSERT_NE(found_prog, nullptr) << "query_events() not found on callback owner object";

    runtime_index = found_prog->function_table[index].runtime_index + fio;
    saved_current_object = current_object;
    saved_vio = variable_index_offset;
    current_object = owner;
    variable_index_offset = vio;
    call_function(owner->prog, runtime_index, 0, &ret);
    current_object = saved_current_object;
    variable_index_offset = saved_vio;

    ASSERT_EQ(ret.type, T_ARRAY) << "query_events() should return an array";
    ASSERT_NE(ret.u.arr, nullptr) << "query_events() returned a null array";

    for (event_index = 0; event_index < ret.u.arr->size; event_index++) {
      svalue_t* event = &ret.u.arr->item[event_index];
      ASSERT_EQ(event->type, T_ARRAY) << "Each callback record should be an array";
      ASSERT_NE(event->u.arr, nullptr) << "Callback record array is null";
      ASSERT_GE(event->u.arr->size, 2) << "Callback record should have at least type and fd";

      svalue_t* event_type = &event->u.arr->item[0];
      svalue_t* event_fd = &event->u.arr->item[1];
      svalue_t* event_payload = event->u.arr->size > 2 ? &event->u.arr->item[2] : nullptr;

      ASSERT_EQ(event_type->type, T_STRING) << "Callback record type should be a string";
      ASSERT_EQ(event_fd->type, T_NUMBER) << "Callback record fd should be a number";

      CallbackRecord::CallbackType type = CallbackRecord::CB_READ;
      if (strcmp(event_type->u.string, "write") == 0) {
        type = CallbackRecord::CB_WRITE;
      } else if (strcmp(event_type->u.string, "close") == 0) {
        type = CallbackRecord::CB_CLOSE;
      }

      std::string payload;
      if (event_payload != nullptr) {
        if (event_payload->type == T_STRING) {
          payload = event_payload->u.string;
        } else if (event_payload->type == T_NUMBER) {
          payload = std::to_string(event_payload->u.number);
        }
      }

      callback_records.emplace(type, static_cast<int>(event_fd->u.number), 0, payload);
    }

    free_svalue(&ret, "CaptureCallbacksFromOwner");
  }

  void ClearCallbackOwnerEvents(object_t* owner) {
    int index;
    int fio;
    int vio;
    int runtime_index;
    int saved_vio;
    object_t* saved_current_object;
    program_t* found_prog;

    ASSERT_NE(owner, nullptr) << "Callback owner object is null";

    found_prog = find_function(owner->prog, findstring("clear_events"), &index, &fio, &vio);
    ASSERT_NE(found_prog, nullptr) << "clear_events() not found on callback owner object";

    runtime_index = found_prog->function_table[index].runtime_index + fio;
    saved_current_object = current_object;
    saved_vio = variable_index_offset;
    current_object = owner;
    variable_index_offset = vio;
    call_function(owner->prog, runtime_index, 0, nullptr);
    current_object = saved_current_object;
    variable_index_offset = saved_vio;
    ClearCallbacks();
  }

  /**
   * @brief Set the current_object context for socket API calls.
   *
   * Socket APIs (socket_create, socket_bind, etc.) use the current_object
   * global to identify the owning object. This saves and restores the
   * previous context.
   *
   * Usage:
   *   ScopedObjectContext ctx(this, master_ob);
   *   int fd = socket_create(...);
   */
  class ScopedObjectContext {
  private:
    object_t* saved_ob;
  public:
    explicit ScopedObjectContext(SocketEfunsBehaviorTest*, object_t* ob)
      : saved_ob(current_object) {
      current_object = ob;
    }
    ~ScopedObjectContext() {
      current_object = saved_ob;
    }
  };
}; // SocketEfunsBehaviorTest
