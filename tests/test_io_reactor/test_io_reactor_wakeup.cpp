/**
 * @file test_io_reactor_wakeup.cpp
 * @brief Tests for io_reactor_wakeup() functionality on Windows.
 *
 * These tests verify that the reactor can be woken up from blocking wait
 * by calling io_reactor_wakeup() from another thread (e.g., timer callback).
 */

#include <gtest/gtest.h>

#ifdef _WIN32

extern "C" {
#include "port/io_reactor.h"
#include <windows.h>
}

#include <thread>
#include <chrono>

/**
 * Test that io_reactor_wakeup() interrupts a blocking wait.
 */
TEST(IOReactorWakeupTest, WakeupFromAnotherThread) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    // Set a long timeout (10 seconds) that we'll interrupt
    struct timeval timeout = {10, 0};
    io_event_t events[10];
    
    // Record start time
    auto start = std::chrono::steady_clock::now();
    
    // Spawn thread to wake us up after 200ms
    std::thread wakeup_thread([reactor]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        io_reactor_wakeup(reactor);
    });
    
    // Wait - should be interrupted by wakeup after ~200ms
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    auto end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    wakeup_thread.join();
    
    // Should return 0 events (wakeup doesn't create events)
    EXPECT_EQ(0, n);
    
    // Should have returned much sooner than 10 seconds (check within 1 second)
    EXPECT_LT(elapsed_ms, 1000);
    
    // Should have taken at least 150ms (allowing some timing variance)
    EXPECT_GT(elapsed_ms, 150);
    
    io_reactor_destroy(reactor);
}

/**
 * Test that multiple wakeups are handled correctly.
 */
TEST(IOReactorWakeupTest, MultipleWakeups) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    struct timeval timeout = {5, 0};
    io_event_t events[10];
    
    // Wake up multiple times from different threads
    std::thread t1([reactor]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        io_reactor_wakeup(reactor);
    });
    
    std::thread t2([reactor]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        io_reactor_wakeup(reactor);
    });
    
    auto start = std::chrono::steady_clock::now();
    
    // First wait should be interrupted by t1 after ~100ms
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    auto mid = std::chrono::steady_clock::now();
    auto first_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(mid - start).count();
    
    EXPECT_EQ(0, n);
    EXPECT_LT(first_elapsed, 500);
    EXPECT_GT(first_elapsed, 50);
    
    // Second wait should return immediately if t2 already signaled
    // or be interrupted shortly by t2
    n = io_reactor_wait(reactor, events, 10, &timeout);
    
    auto end = std::chrono::steady_clock::now();
    auto second_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - mid).count();
    
    EXPECT_EQ(0, n);
    EXPECT_LT(second_elapsed, 500);  // Should not wait full timeout
    
    t1.join();
    t2.join();
    
    io_reactor_destroy(reactor);
}

/**
 * Test that wakeup works correctly when combined with actual socket events.
 */
TEST(IOReactorWakeupTest, WakeupWithSocketEvents) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    // Create a socket pair for testing
    SOCKET pair[2];
    ASSERT_EQ(0, create_test_socket_pair(pair));
    
    // Add one socket to the reactor
    void* context = (void*)0x12345;
    ASSERT_EQ(0, io_reactor_add(reactor, pair[0], context, EVENT_READ));
    
    struct timeval timeout = {5, 0};
    io_event_t events[10];
    
    // Write data to trigger EVENT_READ
    const char* msg = "test";
    send(pair[1], msg, (int)4, 0);
    
    // Also schedule a wakeup
    std::thread wakeup_thread([reactor]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        io_reactor_wakeup(reactor);
    });
    
    // Wait - should return with socket event
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    wakeup_thread.join();
    
    // Should get at least the socket event (wakeup may or may not be visible)
    EXPECT_GE(n, 1);
    
    // Find the socket event
    bool found_socket_event = false;
    for (int i = 0; i < n; i++) {
        if (events[i].context == context) {
            found_socket_event = true;
            EXPECT_NE(0, events[i].event_type & EVENT_READ);
        }
    }
    EXPECT_TRUE(found_socket_event);
    
    io_reactor_remove(reactor, pair[0]);
    closesocket(pair[0]);
    closesocket(pair[1]);
    io_reactor_destroy(reactor);
}

#endif  // _WIN32
