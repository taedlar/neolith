/**
 * @file test_async_queue_threadsafety.cpp
 * @brief Thread safety tests for async_queue
 */

#include <gtest/gtest.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

extern "C" {
#include "async/async_queue.h"
#include "async/async_worker.h"
}

#include <atomic>
#include <cstring>

class AsyncQueueThreadSafetyTest : public ::testing::Test {
protected:
    async_queue_t* queue = nullptr;
    
    void TearDown() override {
        if (queue) {
            async_queue_destroy(queue);
            queue = nullptr;
        }
    }
};

struct ProducerContext {
    async_queue_t* queue;
    int num_messages;
    int producer_id;
};

static void* producer_thread(void* arg) {
    ProducerContext* ctx = (ProducerContext*)arg;
    
    for (int i = 0; i < ctx->num_messages; i++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "P%d-M%d", ctx->producer_id, i);
        
        while (!async_queue_enqueue(ctx->queue, msg, strlen(msg) + 1)) {
            // Retry if queue full
#ifdef _WIN32
            Sleep(1);
#else
            usleep(1000);
#endif
        }
    }
    
    return nullptr;
}

TEST_F(AsyncQueueThreadSafetyTest, SingleProducerSingleConsumer) {
    queue = async_queue_create(32, 128, (async_queue_flags_t)0);
    ASSERT_NE(queue, nullptr);
    
    const int NUM_MESSAGES = 1000;
    
    ProducerContext ctx = { queue, NUM_MESSAGES, 0 };
    async_worker_t* worker = async_worker_create(producer_thread, &ctx, 0);
    ASSERT_NE(worker, nullptr);
    
    // Consumer: read all messages
    int received = 0;
    char buffer[128];
    
    while (received < NUM_MESSAGES) {
        size_t size;
        if (async_queue_dequeue(queue, buffer, sizeof(buffer), &size)) {
            received++;
        } else {
            // Queue empty, yield
#ifdef _WIN32
            Sleep(1);
#else
            usleep(1000);
#endif
        }
    }
    
    EXPECT_EQ(received, NUM_MESSAGES);
    
    async_worker_signal_stop(worker);
    async_worker_join(worker, 5000);
    async_worker_destroy(worker);
}

TEST_F(AsyncQueueThreadSafetyTest, MultipleProducers) {
    queue = async_queue_create(256, 128, ASYNC_QUEUE_DROP_OLDEST);
    ASSERT_NE(queue, nullptr);
    
    const int NUM_PRODUCERS = 4;
    const int MESSAGES_PER_PRODUCER = 100;
    
    ProducerContext contexts[NUM_PRODUCERS];
    async_worker_t* workers[NUM_PRODUCERS];
    
    // Start producers
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        contexts[i] = { queue, MESSAGES_PER_PRODUCER, i };
        workers[i] = async_worker_create(producer_thread, &contexts[i], 0);
        ASSERT_NE(workers[i], nullptr);
    }
    
    // Give producers time to send messages
#ifdef _WIN32
    Sleep(500);  // Let them work for a bit
#else
    usleep(500000);
#endif
    
    // Consumer: drain queue
    int received = 0;
    char buffer[128];
    size_t size;
    
    while (async_queue_dequeue(queue, buffer, sizeof(buffer), &size)) {
        received++;
    }
    
    // We should have received many messages (most should fit in the larger queue)
    const int EXPECTED_TOTAL = NUM_PRODUCERS * MESSAGES_PER_PRODUCER;
    EXPECT_GT(received, EXPECTED_TOTAL / 4);  // At least 25% should succeed
    
    // Clean up workers
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        async_worker_signal_stop(workers[i]);
        async_worker_join(workers[i], 5000);
        async_worker_destroy(workers[i]);
    }
}
