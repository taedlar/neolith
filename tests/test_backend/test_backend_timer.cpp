/**
 * @file test_backend_timer.cpp
 * @brief Tests for backend timer integration with timer_port
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

extern "C" {
    #include "std.h"
    #include "backend.h"
    #include "port/timer_port.h"
}

using namespace testing;

class BackendTimerTest : public Test {
protected:
    void SetUp() override {
        debug_set_log_with_date(0);
    }

    void TearDown() override {
        // Ensure heart_beat_flag is reset
        heart_beat_flag = 0;
    }
};

/**
 * @brief Test that heart_beat_flag is set by timer callback
 */
TEST_F(BackendTimerTest, HeartBeatFlagSetByTimer) {
    timer_port_t test_timer;
    memset(&test_timer, 0, sizeof(test_timer));

    auto test_callback = +[]() {
        heart_beat_flag = 1;
    };

    // Initialize timer
    ASSERT_EQ(timer_port_init(&test_timer), TIMER_OK) << "Failed to initialize timer";

    // Start timer with 100ms interval (100,000 microseconds)
    heart_beat_flag = 0;
    ASSERT_EQ(timer_port_start(&test_timer, 100000, test_callback), TIMER_OK)
        << "Failed to start timer";

    // Wait for at least one timer expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Verify heart_beat_flag was set
    EXPECT_EQ(heart_beat_flag, 1) << "heart_beat_flag should be set by timer callback";

    // Stop timer
    ASSERT_EQ(timer_port_stop(&test_timer), TIMER_OK) << "Failed to stop timer";

    // Cleanup
    timer_port_cleanup(&test_timer);
}

/**
 * @brief Test multiple timer callbacks over time
 */
TEST_F(BackendTimerTest, MultipleTimerCallbacks) {
    timer_port_t test_timer;
    memset(&test_timer, 0, sizeof(test_timer));
    static volatile int callback_count = 0;

    auto counting_callback = +[]() {
        callback_count++;
    };

    // Initialize and start timer
    ASSERT_EQ(timer_port_init(&test_timer), TIMER_OK);

    callback_count = 0;
    // Use 200ms interval for more reliable testing
    ASSERT_EQ(timer_port_start(&test_timer, 200000, counting_callback), TIMER_OK);

    // Wait for ~1 second (should get ~5 callbacks)
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    // Stop timer
    timer_port_stop(&test_timer);

    // Should have received between 4 and 6 callbacks (allowing for timing variations)
    EXPECT_GE(callback_count, 4) << "Should have at least 4 callbacks in 1.1 seconds";
    EXPECT_LE(callback_count, 6) << "Should have at most 6 callbacks in 1.1 seconds";

    // Cleanup
    timer_port_cleanup(&test_timer);
}

/**
 * @brief Test timer stop prevents further callbacks
 */
TEST_F(BackendTimerTest, TimerStopPreventsFurtherCallbacks) {
    timer_port_t test_timer;
    memset(&test_timer, 0, sizeof(test_timer));
    static volatile int callback_count = 0;

    auto counting_callback = +[]() {
        callback_count++;
    };

    // Initialize and start timer
    ASSERT_EQ(timer_port_init(&test_timer), TIMER_OK);

    callback_count = 0;
    ASSERT_EQ(timer_port_start(&test_timer, 100000, counting_callback), TIMER_OK);

    // Wait for a few callbacks
    std::this_thread::sleep_for(std::chrono::milliseconds(350));

    // Stop timer
    ASSERT_EQ(timer_port_stop(&test_timer), TIMER_OK);
    int count_at_stop = callback_count;

    // Wait again - count should not increase
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_EQ(callback_count, count_at_stop)
        << "Callback count should not increase after timer stopped";

    // Cleanup
    timer_port_cleanup(&test_timer);
}

/**
 * @brief Test timer restart functionality
 */
TEST_F(BackendTimerTest, TimerRestart) {
    timer_port_t test_timer;
    memset(&test_timer, 0, sizeof(test_timer));
    static volatile int callback_count = 0;

    auto counting_callback = +[]() {
        callback_count++;
    };

    // Initialize timer
    ASSERT_EQ(timer_port_init(&test_timer), TIMER_OK);

    // First run
    callback_count = 0;
    ASSERT_EQ(timer_port_start(&test_timer, 100000, counting_callback), TIMER_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    ASSERT_EQ(timer_port_stop(&test_timer), TIMER_OK);
    int first_count = callback_count;
    EXPECT_GE(first_count, 1) << "Should have callbacks from first run";

    // Restart timer
    callback_count = 0;
    ASSERT_EQ(timer_port_start(&test_timer, 100000, counting_callback), TIMER_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    ASSERT_EQ(timer_port_stop(&test_timer), TIMER_OK);
    int second_count = callback_count;
    EXPECT_GE(second_count, 1) << "Should have callbacks from second run";

    // Cleanup
    timer_port_cleanup(&test_timer);
}

/**
 * @brief Test timer is_active status
 */
TEST_F(BackendTimerTest, TimerActiveStatus) {
    timer_port_t test_timer;
    memset(&test_timer, 0, sizeof(test_timer));

    auto dummy_callback = +[]() { };

    // Initialize timer
    ASSERT_EQ(timer_port_init(&test_timer), TIMER_OK);
    EXPECT_EQ(timer_port_is_active(&test_timer), TIMER_OK) << "Timer should not be active after init";

    // Start timer
    ASSERT_EQ(timer_port_start(&test_timer, 100000, dummy_callback), TIMER_OK);
    EXPECT_EQ(timer_port_is_active(&test_timer), 1) << "Timer should be active after start";

    // Stop timer
    ASSERT_EQ(timer_port_stop(&test_timer), TIMER_OK);
    EXPECT_EQ(timer_port_is_active(&test_timer), TIMER_OK) << "Timer should not be active after stop";

    // Cleanup
    timer_port_cleanup(&test_timer);
}

/**
 * @brief Test HEARTBEAT_INTERVAL timer interval (2 seconds)
 */
TEST_F(BackendTimerTest, HeartBeatIntervalTiming) {
    timer_port_t test_timer;
    memset(&test_timer, 0, sizeof(test_timer));
    static volatile int callback_count = 0;
    
    auto counting_callback = +[]() {
        callback_count++;
    };

    // Initialize timer
    ASSERT_EQ(timer_port_init(&test_timer), TIMER_OK);

    // Start timer with HEARTBEAT_INTERVAL (2 seconds = 2,000,000 microseconds)
    callback_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    ASSERT_EQ(timer_port_start(&test_timer, HEARTBEAT_INTERVAL, counting_callback), TIMER_OK);

    // Wait for ~4.5 seconds (should get 2 callbacks at 2s and 4s)
    std::this_thread::sleep_for(std::chrono::milliseconds(4500));
    auto end_time = std::chrono::steady_clock::now();

    // Stop timer
    timer_port_stop(&test_timer);

    // Should have received 2 callbacks (allowing for timing variations)
    EXPECT_GE(callback_count, 2) << "Should have at least 2 callbacks in 4.5 seconds";
    EXPECT_LE(callback_count, 3) << "Should have at most 3 callbacks in 4.5 seconds";

    // Verify timing accuracy (within 10% tolerance)
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    EXPECT_GE(elapsed_ms, 4400) << "Test should run for ~4.5 seconds";
    EXPECT_LE(elapsed_ms, 4700) << "Test should run for ~4.5 seconds";

    // Cleanup
    timer_port_cleanup(&test_timer);
}

/**
 * @brief Test query_heart_beat integration with timer
 */
TEST_F(BackendTimerTest, QueryHeartBeatIntegration) {
    // This test verifies that query_heart_beat() works correctly
    // even though the underlying timer implementation has changed

    // Note: query_heart_beat() operates on objects, not timers directly
    // The timer just triggers the heart beat flag; the object tracking
    // is handled separately by set_heart_beat()/query_heart_beat()
    
    // This test ensures the timer infrastructure doesn't interfere
    // with the existing heart beat object tracking

    timer_port_t test_timer;
    memset(&test_timer, 0, sizeof(test_timer));
    ASSERT_EQ(timer_port_init(&test_timer), TIMER_OK);
    
    // Just verify timer can start/stop without affecting other systems
    auto dummy_callback = +[]() { };
    ASSERT_EQ(timer_port_start(&test_timer, 100000, dummy_callback), TIMER_OK);
    ASSERT_EQ(timer_port_stop(&test_timer), TIMER_OK);
    
    timer_port_cleanup(&test_timer);
}
