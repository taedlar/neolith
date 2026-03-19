#pragma once

#include <gtest/gtest.h>
#include <filesystem>
#include <queue>
#include <memory>

extern "C" {
  #include "std.h"
  #include "rc.h"
  #include "lpc/compiler.h"
  #include "lpc/types.h"
  #include "lpc/object.h"
  #include "lpc/functional.h"
  #include "socket/socket_efuns.h"
  #include "lpc/include/socket_err.h"
  #include "port/socket_comm.h"
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
 * The LPC runtime (strings, compiler, simulate, master) is initialized once
 * per test suite via SetUpTestSuite/TearDownTestSuite to avoid heap corruption
 * from repeated init/deinit cycles on Windows. Per-test SetUp only resets the
 * lightweight callback queue.
 */
class SocketEfunsBehaviorTest : public Test {
protected:
  // Initialized once per suite; persists across all tests.
  inline static std::filesystem::path s_previous_cwd;

  std::queue<CallbackRecord> callback_records; // ordered callback history

  static void SetUpTestSuite() {
    // Initialize logging and locale
    debug_set_log_with_date(0);
    setlocale(LC_ALL, PLATFORM_UTF8_LOCALE);

    // Initialize core driver
    init_stem(3, (unsigned long)-1, "m3.conf");
    init_config(MAIN_OPTION(config_file));
    debug_message("[ SETUP    ] CTEST_FULL_OUTPUT");

    // Verify mudlib path and change to it
    ASSERT_TRUE(CONFIG_STR(__MUD_LIB_DIR__)) << "Mudlib directory not configured";
    namespace fs = std::filesystem;
    auto mudlib_path = fs::path(CONFIG_STR(__MUD_LIB_DIR__));
    if (mudlib_path.is_relative()) {
      mudlib_path = fs::current_path() / mudlib_path;
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
  }

  static void TearDownTestSuite() {
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
    fs::current_path(s_previous_cwd);
  }

  void SetUp() override {
    // Clear callback queue before each test.
    ClearCallbacks();
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
