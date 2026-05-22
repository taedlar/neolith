#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "rc.h"
#include "src/backend.h"
#include "src/comm.h"
#include "src/command.h"
#include "lpc/compiler.h"

#include <gtest/gtest.h>
#include <filesystem>

using namespace testing;

namespace {

class ConsoleQueueDrainIntegrationTest : public Test {
private:
  std::filesystem::path previous_cwd;

protected:
  void SetUp() override {
    namespace fs = std::filesystem;
    previous_cwd = fs::current_path();
    setlocale(LC_ALL, PLATFORM_UTF8_LOCALE);
    debug_set_log_with_date(false);

    fs::path config_dir = fs::current_path();
    if (!fs::exists(config_dir / "m3.conf"))
      {
        fs::current_path(config_dir.parent_path());
      }

    init_stem(3, (unsigned long)-1, "m3.conf");
    MAIN_OPTION(pedantic) = true;
    MAIN_OPTION(trace_flags) = 0;

    init_config(MAIN_OPTION(config_file));
    init_strings(8192, 1000000);
    init_lpc_compiler(CONFIG_INT(__MAX_LOCAL_VARIABLES__), CONFIG_STR(__INCLUDE_DIRS__));
    setup_simulate();
  }

  void TearDown() override {
    tear_down_simulate();
    deinit_lpc_compiler();
    deinit_strings();
    deinit_config();

    namespace fs = std::filesystem;
    fs::current_path(previous_cwd);
  }
};

TEST_F(ConsoleQueueDrainIntegrationTest, ProcessIoDrainsQueueAndMarksCommandReady) {
  async_queue_t *saved_queue = g_console_queue;
  interactive_t **saved_users = all_users;
  int saved_max_users = max_users;

  g_console_queue = async_queue_create(8, CONSOLE_MAX_LINE, (async_queue_flags_t)0);
  ASSERT_NE(g_console_queue, nullptr);

  interactive_t console_ip = {};
  interactive_t *users[1] = {&console_ip};
  all_users = users;
  max_users = 1;

  static const char kLine[] = "ping\n";
  ASSERT_TRUE(async_queue_enqueue(g_console_queue, kLine, sizeof(kLine)));

  process_io();

  EXPECT_TRUE(async_queue_is_empty(g_console_queue));
  EXPECT_TRUE(console_ip.iflags & CMD_IN_BUF);
  EXPECT_STREQ(console_ip.text, "ping");
  EXPECT_EQ(cmd_in_buf(&console_ip), 1);

  async_queue_destroy(g_console_queue);
  g_console_queue = saved_queue;
  all_users = saved_users;
  max_users = saved_max_users;
}

}  // namespace
