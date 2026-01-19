/**
 * @file test_async_worker_lifecycle.cpp
 * @brief Worker thread lifecycle tests
 */

#include <gtest/gtest.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

extern "C" {
#include "async/async_worker.h"
}

#include <atomic>

class AsyncWorkerLifecycleTest : public ::testing::Test {
};

static void* simple_worker(void* arg) {
    int* counter = (int*)arg;
    (*counter)++;
    return nullptr;
}

TEST_F(AsyncWorkerLifecycleTest, CreateDestroy) {
    int counter = 0;
    async_worker_t* worker = async_worker_create(simple_worker, &counter, 0);
    ASSERT_NE(worker, nullptr);
    
    // Wait for worker to finish
    bool joined = async_worker_join(worker, 5000);
    EXPECT_TRUE(joined);
    EXPECT_EQ(counter, 1);
    
    async_worker_destroy(worker);
}

static void* looping_worker(void* arg) {
    async_worker_t* self = async_worker_current();
    int* iterations = (int*)arg;
    
    while (!async_worker_should_stop(self)) {
        (*iterations)++;
        
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
    
    return nullptr;
}

TEST_F(AsyncWorkerLifecycleTest, GracefulShutdown) {
    int iterations = 0;
    async_worker_t* worker = async_worker_create(looping_worker, &iterations, 0);
    ASSERT_NE(worker, nullptr);
    
    // Let it run for a bit
#ifdef _WIN32
    Sleep(100);
#else
    usleep(100000);
#endif
    
    int iter_count_before_stop = iterations;
    EXPECT_GT(iter_count_before_stop, 0);
    
    // Signal stop
    async_worker_signal_stop(worker);
    
    // Wait for graceful exit
    bool joined = async_worker_join(worker, 5000);
    EXPECT_TRUE(joined);
    
    // Worker should have stopped
    EXPECT_EQ(async_worker_get_state(worker), ASYNC_WORKER_STOPPED);
    
    async_worker_destroy(worker);
}

TEST_F(AsyncWorkerLifecycleTest, JoinTimeout) {
    struct InfiniteLoopContext {
        std::atomic<bool> started{false};
    };
    
    auto infinite_worker = [](void* arg) -> void* {
        InfiniteLoopContext* ctx = (InfiniteLoopContext*)arg;
        ctx->started.store(true);
        
        // Infinite loop (ignores should_stop)
        while (true) {
#ifdef _WIN32
            Sleep(100);
#else
            usleep(100000);
#endif
        }
        return nullptr;
    };
    
    InfiniteLoopContext ctx;
    async_worker_t* worker = async_worker_create(infinite_worker, &ctx, 0);
    ASSERT_NE(worker, nullptr);
    
    // Wait for worker to start
    while (!ctx.started.load()) {
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
    
    async_worker_signal_stop(worker);
    
    // Join should timeout
    bool joined = async_worker_join(worker, 100);
    EXPECT_FALSE(joined);
    
    // Note: Worker is leaked here since it won't exit
    // In production, this would require forced termination
}

static void* worker_with_context(void* arg) {
    const char* message = (const char*)arg;
    // Verify context is valid
    EXPECT_STREQ(message, "test context");
    return (void*)42;
}

TEST_F(AsyncWorkerLifecycleTest, WorkerContext) {
    const char* ctx = "test context";
    async_worker_t* worker = async_worker_create(worker_with_context, (void*)ctx, 0);
    ASSERT_NE(worker, nullptr);
    
    bool joined = async_worker_join(worker, 5000);
    EXPECT_TRUE(joined);
    
    async_worker_destroy(worker);
}

static void* state_checking_worker(void* arg) {
    async_worker_t* self = async_worker_current();
    EXPECT_NE(self, nullptr);
    
    EXPECT_EQ(async_worker_get_state(self), ASYNC_WORKER_RUNNING);
    
    while (!async_worker_should_stop(self)) {
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
    
    return nullptr;
}

TEST_F(AsyncWorkerLifecycleTest, WorkerState) {
    async_worker_t* worker = async_worker_create(state_checking_worker, nullptr, 0);
    ASSERT_NE(worker, nullptr);
    
    // Worker should be running
#ifdef _WIN32
    Sleep(50);
#else
    usleep(50000);
#endif
    
    async_worker_state_t state = async_worker_get_state(worker);
    EXPECT_EQ(state, ASYNC_WORKER_RUNNING);
    
    async_worker_signal_stop(worker);
    async_worker_join(worker, 5000);
    
    EXPECT_EQ(async_worker_get_state(worker), ASYNC_WORKER_STOPPED);
    
    async_worker_destroy(worker);
}
