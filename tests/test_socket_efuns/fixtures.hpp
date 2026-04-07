#pragma once

#include <gtest/gtest.h>
#include <filesystem>
#include <queue>
#include <memory>
#include "lpc/types.hpp"

extern "C" {
  #include "std.h"
  #include "rc.h"
  #include "async/async_runtime.h"
  #include "port/socket_comm.h"
  #include "lpc/compiler.h"
  #include "lpc/array.h"
  #include "lpc/program.h"
  #include "lpc/types.h"
  #include "lpc/object.h"
  #include "socket/socket_efuns.h"
  #include "lpc/include/socket_err.h"
}

/**
 * @brief Poll DNS completion state for a socket operation.
 *
 * Repeatedly drains resolver completions and checks operation phase until
 * the socket transitions out of DNS resolving or timeout is reached.
 *
 * @param socket_fd Socket descriptor tracked by socket operation state.
 * @param timeout_ms Maximum time to wait in milliseconds.
 * @returns true when DNS work completes, false on timeout.
 */
static inline bool WaitForDNSCompletion(int socket_id, int timeout_ms = 5000) {
  int elapsed = 0;
  const int sleep_step = 10;

  while (elapsed < timeout_ms) {
    handle_dns_completions();

    int op_active = 0;
    int op_terminal = 0;
    int op_id = 0;
    int op_phase = 0;
    if (get_socket_operation_info(socket_id, &op_active, &op_terminal,
                                  &op_id, &op_phase) == EESUCCESS) {
      if (op_terminal || op_phase != OP_DNS_RESOLVING) {
        return true;
      }
    }

#ifdef WINSOCK
    Sleep(sleep_step);
#else
    usleep(sleep_step * 1000);
#endif
    elapsed += sleep_step;
  }

  return false;
}

using namespace testing;

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
    MAIN_OPTION(pedantic) = 1; // enable pedantic mode for stricter checks
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

    // Initialize async runtime (needed for socket operations and DNS)
    if (g_runtime == nullptr) {
      g_runtime = async_runtime_init();
      ASSERT_NE(g_runtime, nullptr) << "Failed to initialize async runtime";
    }

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

    // Deinitialize async runtime
    if (g_runtime != nullptr) {
      async_runtime_deinit(g_runtime);
      g_runtime = nullptr;
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

    found_prog = find_function(owner->prog, findstring("query_events", NULL), &index, &fio, &vio);
    ASSERT_NE(found_prog, nullptr) << "query_events() not found on callback owner object";

    runtime_index = found_prog->function_table[index].runtime_index + fio;
    saved_current_object = current_object;
    saved_vio = variable_index_offset;
    current_object = owner;
    variable_index_offset = vio;
    call_function(owner->prog, runtime_index, 0, &ret);
    current_object = saved_current_object;
    variable_index_offset = saved_vio;

    ASSERT_TRUE(lpc::svalue_view::from(&ret).is_array()) << "query_events() should return an array";
    ASSERT_NE(ret.u.arr, nullptr) << "query_events() returned a null array";

    for (event_index = 0; event_index < ret.u.arr->size; event_index++) {
      svalue_t* event = &ret.u.arr->item[event_index];
      ASSERT_TRUE(lpc::svalue_view::from(event).is_array()) << "Each callback record should be an array";
      ASSERT_NE(event->u.arr, nullptr) << "Callback record array is null";
      ASSERT_GE(event->u.arr->size, 2) << "Callback record should have at least type and fd";

      svalue_t* event_type = &event->u.arr->item[0];
      svalue_t* event_fd = &event->u.arr->item[1];
      svalue_t* event_payload = event->u.arr->size > 2 ? &event->u.arr->item[2] : nullptr;

      ASSERT_TRUE(lpc::svalue_view::from(event_type).is_string()) << "Callback record type should be a string";
      ASSERT_TRUE(lpc::svalue_view::from(event_fd).is_number()) << "Callback record fd should be a number";

      CallbackRecord::CallbackType type = CallbackRecord::CB_READ;
      auto event_type_view = lpc::svalue_view::from(event_type);
      ASSERT_TRUE(event_type_view.is_string()) << "Callback record type should be a string";
      if (strcmp(event_type_view.c_str(), "write") == 0) {
        type = CallbackRecord::CB_WRITE;
      } else if (strcmp(event_type_view.c_str(), "close") == 0) {
        type = CallbackRecord::CB_CLOSE;
      }

      std::string payload;
      if (event_payload != nullptr) {
        if (lpc::svalue_view::from(event_payload).is_string()) {
            auto event_payload_view = lpc::svalue_view::from(event_payload);
            ASSERT_TRUE(event_payload_view.is_string());
            payload = event_payload_view.c_str();
        } else if (lpc::svalue_view::from(event_payload).is_number()) {
          payload = std::to_string(lpc::svalue_view::from(event_payload).number());
        }
      }

      callback_records.emplace(type, static_cast<int>(lpc::svalue_view::from(event_fd).number()), 0, payload);
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

    found_prog = find_function(owner->prog, findstring("clear_events", NULL), &index, &fio, &vio);
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
   * @brief OP_PHASES - Socket operation lifecycle phases for state tracking.
   */
  static constexpr int OP_INIT = 0;
  static constexpr int OP_DNS_RESOLVING = 1;
  static constexpr int OP_CONNECTING = 2;
  static constexpr int OP_TRANSFERRING = 3;
  static constexpr int OP_COMPLETED = 4;
  static constexpr int OP_FAILED = 5;
  static constexpr int OP_TIMED_OUT = 6;
  static constexpr int OP_CANCELED = 7;

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

  /**
   * @brief RAII guard for DNS timeout test hook.
   *
   * Sets a test hook that forces DNS operations to timeout on construction,
   * clears it on destruction. Enables deterministic DNS timeout testing.
   *
   * Usage:
   *   {
   *     ScopedDnsTimeoutHook timeout_hook;
   *     // DNS operations will timeout within this scope
   *   }
   */
  class ScopedDnsTimeoutHook {
  public:
    ScopedDnsTimeoutHook() {
      set_socket_dns_timeout_test_hook(_ForceDnsTimeoutHookImpl);
    }

    ~ScopedDnsTimeoutHook() {
      set_socket_dns_timeout_test_hook(nullptr);
    }

  private:
    static int _ForceDnsTimeoutHookImpl(int, const char *, uint16_t) {
      return 1;
    }
  };

  /**
   * @brief RAII guard for resolver lookup test hook.
   *
   * Sets a custom test hook for resolver DNS queries on construction,
   * clears it on destruction. Enables deterministic DNS behavior testing.
   *
   * Usage:
   *   ScopedResolverLookupHook hook(my_hook_function);
   *   // Resolver queries will use my_hook_function
   */
  class ScopedResolverLookupHook {
  public:
    explicit ScopedResolverLookupHook(void (*hook)(const char *, unsigned int *, const char **)) {
      addr_resolver_set_lookup_test_hook(hook);
    }

    ~ScopedResolverLookupHook() {
      addr_resolver_set_lookup_test_hook(nullptr);
    }
  };

  /**
   * @brief RAII guard for resolver initialization and cleanup.
   *
   * Manages resolver lifecycle for tests that validate DNS resolution behavior.
   * The async_runtime must already be initialized (by the test fixture).
   *
   * On construction: deinitializes any existing resolver (to reset telemetry to zero)
   * and initializes a fresh resolver with the configured backend.
   *
   * On destruction: deinitializes the resolver to clean up worker threads and state.
   */
  class ScopedResolver {
  public:
    ScopedResolver() : resolver_ready_(false) {
      if (g_runtime != nullptr) {
        // Deinit any existing resolver to reset telemetry counters
        addr_resolver_deinit();

        // Initialize fresh resolver from config
        addr_resolver_config_t resolver_config;
        stem_get_addr_resolver_config(&resolver_config);
        resolver_ready_ = (addr_resolver_init(g_runtime, &resolver_config) != 0);
      }
    }

    ~ScopedResolver() {
      addr_resolver_deinit();
    }

    bool IsReady() const {
      return g_runtime != nullptr && resolver_ready_;
    }

  private:
    bool resolver_ready_;
  };

}; // SocketEfunsBehaviorTest

/* OP_PHASES definitions for all socket tests */
#define OP_INIT 0
#define OP_DNS_RESOLVING 1
#define OP_CONNECTING 2
#define OP_TRANSFERRING 3
#define OP_COMPLETED 4
#define OP_FAILED 5
#define OP_TIMED_OUT 6
#define OP_CANCELED 7
