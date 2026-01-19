/**
 * @file test_console_worker_lifecycle.cpp
 * @brief Tests for console worker lifecycle management
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

extern "C" {
#include "async/console_worker.h"
#include "async/async_runtime.h"
#include "async/async_queue.h"
}

/**
 * Test console worker initialization and shutdown
 */
TEST(ConsoleWorkerLifecycle, InitShutdown) {
    async_runtime_t* runtime = async_runtime_init();
    ASSERT_NE(runtime, nullptr);
    
    async_queue_t* queue = async_queue_create(64, CONSOLE_MAX_LINE, (async_queue_flags_t)0);
    ASSERT_NE(queue, nullptr);
    
    console_worker_context_t* ctx = console_worker_init(runtime, queue, CONSOLE_COMPLETION_KEY);
    ASSERT_NE(ctx, nullptr);
    
    /* Worker may or may not be created depending on console type */
    if (ctx->console_type != CONSOLE_TYPE_NONE) {
        EXPECT_NE(ctx->worker, nullptr);
    }
    
    /* Shutdown with timeout */
    bool stopped = console_worker_shutdown(ctx, 5000);
    EXPECT_TRUE(stopped);
    
    console_worker_destroy(ctx);
    async_queue_destroy(queue);
    async_runtime_deinit(runtime);
}

/**
 * Test console worker with NULL arguments
 */
TEST(ConsoleWorkerLifecycle, NullArguments) {
    async_runtime_t* runtime = async_runtime_init();
    ASSERT_NE(runtime, nullptr);
    
    async_queue_t* queue = async_queue_create(64, CONSOLE_MAX_LINE, (async_queue_flags_t)0);
    ASSERT_NE(queue, nullptr);
    
    /* NULL runtime */
    EXPECT_EQ(console_worker_init(nullptr, queue, CONSOLE_COMPLETION_KEY), nullptr);
    
    /* NULL queue */
    EXPECT_EQ(console_worker_init(runtime, nullptr, CONSOLE_COMPLETION_KEY), nullptr);
    
    async_queue_destroy(queue);
    async_runtime_deinit(runtime);
}

/**
 * Test console worker shutdown without worker
 */
TEST(ConsoleWorkerLifecycle, ShutdownNoWorker) {
    /* Create context with no worker (simulated) */
    console_worker_context_t ctx = {0};
    ctx.console_type = CONSOLE_TYPE_NONE;
    ctx.worker = nullptr;
    
    /* Should return true (no worker to shutdown) */
    EXPECT_TRUE(console_worker_shutdown(&ctx, 1000));
}

/**
 * Test console worker destroy with NULL
 */
TEST(ConsoleWorkerLifecycle, DestroyNull) {
    /* Should not crash */
    console_worker_destroy(nullptr);
}
