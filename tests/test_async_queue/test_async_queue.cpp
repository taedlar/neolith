/**
 * @file test_async_queue_basic.cpp
 * @brief Basic async_queue functionality tests
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"
#include <cstring>

#ifdef _WIN32
class WinsockEnvironment : public ::testing::Environment {
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

static ::testing::Environment* const winsock_env =
    ::testing::AddGlobalTestEnvironment(new WinsockEnvironment);
#endif

TEST_F(AsyncQueueTest, CreateDestroy) {
    queue = async_queue_create(16, 128, (async_queue_flags_t)0);
    ASSERT_NE(queue, nullptr);
    
    EXPECT_TRUE(async_queue_is_empty(queue));
    EXPECT_FALSE(async_queue_is_full(queue));
}

TEST_F(AsyncQueueTest, EnqueueDequeue) {
    queue = async_queue_create(16, 128, (async_queue_flags_t)0);
    ASSERT_NE(queue, nullptr);
    
    const char* msg = "Hello, World!";
    EXPECT_TRUE(async_queue_enqueue(queue, msg, strlen(msg) + 1));
    
    EXPECT_FALSE(async_queue_is_empty(queue));
    
    char buffer[128];
    size_t size = 0;
    EXPECT_TRUE(async_queue_dequeue(queue, buffer, sizeof(buffer), &size));
    EXPECT_EQ(size, strlen(msg) + 1);
    EXPECT_STREQ(buffer, msg);
    
    EXPECT_TRUE(async_queue_is_empty(queue));
}

TEST_F(AsyncQueueTest, MultipleMessages) {
    queue = async_queue_create(8, 64, (async_queue_flags_t)0);
    ASSERT_NE(queue, nullptr);
    
    // Enqueue multiple messages
    for (int i = 0; i < 5; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Message %d", i);
        EXPECT_TRUE(async_queue_enqueue(queue, msg, strlen(msg) + 1));
    }
    
    // Dequeue and verify order
    for (int i = 0; i < 5; i++) {
        char buffer[64];
        char expected[64];
        snprintf(expected, sizeof(expected), "Message %d", i);
        
        size_t size = 0;
        EXPECT_TRUE(async_queue_dequeue(queue, buffer, sizeof(buffer), &size));
        EXPECT_STREQ(buffer, expected);
    }
    
    EXPECT_TRUE(async_queue_is_empty(queue));
}

TEST_F(AsyncQueueTest, QueueFull) {
    queue = async_queue_create(4, 32, (async_queue_flags_t)0);
    ASSERT_NE(queue, nullptr);
    
    // Fill queue to capacity
    for (int i = 0; i < 4; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Msg%d", i);
        EXPECT_TRUE(async_queue_enqueue(queue, msg, strlen(msg) + 1));
    }
    
    EXPECT_TRUE(async_queue_is_full(queue));
    
    // Next enqueue should fail
    const char* overflow = "Overflow";
    EXPECT_FALSE(async_queue_enqueue(queue, overflow, strlen(overflow) + 1));
}

TEST_F(AsyncQueueTest, DropOldest) {
    queue = async_queue_create(4, 32, ASYNC_QUEUE_DROP_OLDEST);
    ASSERT_NE(queue, nullptr);
    
    // Fill queue
    for (int i = 0; i < 4; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Msg%d", i);
        EXPECT_TRUE(async_queue_enqueue(queue, msg, strlen(msg) + 1));
    }
    
    // Enqueue one more (should drop oldest)
    const char* new_msg = "Msg4";
    EXPECT_TRUE(async_queue_enqueue(queue, new_msg, strlen(new_msg) + 1));
    
    // First message should be Msg1 (Msg0 was dropped)
    char buffer[32];
    EXPECT_TRUE(async_queue_dequeue(queue, buffer, sizeof(buffer), nullptr));
    EXPECT_STREQ(buffer, "Msg1");
}

TEST_F(AsyncQueueTest, Statistics) {
    queue = async_queue_create(8, 64, (async_queue_flags_t)0);
    ASSERT_NE(queue, nullptr);
    
    async_queue_stats_t stats;
    async_queue_get_stats(queue, &stats);
    
    EXPECT_EQ(stats.capacity, 8);
    EXPECT_EQ(stats.max_msg_size, 64);
    EXPECT_EQ(stats.current_size, 0);
    EXPECT_EQ(stats.enqueue_count, 0);
    EXPECT_EQ(stats.dequeue_count, 0);
    
    // Enqueue and dequeue
    const char* msg = "Test";
    async_queue_enqueue(queue, msg, strlen(msg) + 1);
    
    async_queue_get_stats(queue, &stats);
    EXPECT_EQ(stats.current_size, 1);
    EXPECT_EQ(stats.enqueue_count, 1);
    
    char buffer[64];
    async_queue_dequeue(queue, buffer, sizeof(buffer), nullptr);
    
    async_queue_get_stats(queue, &stats);
    EXPECT_EQ(stats.current_size, 0);
    EXPECT_EQ(stats.dequeue_count, 1);
}

TEST_F(AsyncQueueTest, ClearQueue) {
    queue = async_queue_create(8, 64, (async_queue_flags_t)0);
    ASSERT_NE(queue, nullptr);
    
    // Add messages
    for (int i = 0; i < 5; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Msg%d", i);
        async_queue_enqueue(queue, msg, strlen(msg) + 1);
    }
    
    EXPECT_FALSE(async_queue_is_empty(queue));
    
    async_queue_clear(queue);
    
    EXPECT_TRUE(async_queue_is_empty(queue));
}
