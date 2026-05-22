#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <gtest/gtest.h>

#include "src/comm.h"
#include "async/async_queue.h"
#include "async/console_worker.h"

extern "C" {
extern async_queue_t *g_console_queue;
}

namespace {

struct ConsoleGlobalsGuard {
  async_queue_t *saved_queue;
  interactive_t **saved_users;
  int saved_max_users;

  ConsoleGlobalsGuard()
      : saved_queue(g_console_queue),
        saved_users(all_users),
        saved_max_users(max_users) {
  }

  ~ConsoleGlobalsGuard() {
    if (g_console_queue && g_console_queue != saved_queue)
      {
        async_queue_destroy(g_console_queue);
      }

    g_console_queue = saved_queue;
    all_users = saved_users;
    max_users = saved_max_users;
  }
};

}  // namespace

TEST(ConsoleWorkerQueueDrain, DrainsQueuedLineWithoutCompletionEvent) {
  ConsoleGlobalsGuard guard;

  g_console_queue = async_queue_create(8, CONSOLE_MAX_LINE, (async_queue_flags_t)0);
  ASSERT_NE(g_console_queue, nullptr);

  interactive_t console_ip = {};
  interactive_t *users[1] = {&console_ip};
  all_users = users;
  max_users = 1;

  static const char kLine[] = "ping";
  ASSERT_TRUE(async_queue_enqueue(g_console_queue, kLine, sizeof(kLine)));
  EXPECT_FALSE(async_queue_is_empty(g_console_queue));

  /* No completion event is staged. process_io() should still drain queue. */
  process_io();
  EXPECT_TRUE(async_queue_is_empty(g_console_queue));
}
