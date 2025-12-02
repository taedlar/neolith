/**
 * @file test_timer_port.c
 * @brief Unit tests for the timer_port cross-platform timer implementation
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep((x) * 1000)
#else
#include <unistd.h>
#endif

#include "timer_port.h"

/* Test state */
static volatile int test_callback_count = 0;
static volatile int test_running = 1;
static time_t test_start_time;

/**
 * @brief Test callback function
 */
static void test_timer_callback(void)
{
    test_callback_count++;
    printf("Timer callback #%d (elapsed: %ld seconds)\n", 
           test_callback_count, 
           time(NULL) - test_start_time);
}

/**
 * @brief Test basic timer functionality
 */
int test_basic_timer_operation(void)
{
    timer_port_t timer;
    int result;
    
    printf("=== Testing Basic Timer Operation ===\n");
    
    /* Initialize timer */
    result = timer_port_init(&timer);
    if (result != 0) {
        printf("FAIL: timer_port_init() returned %d\n", result);
        return -1;
    }
    printf("PASS: timer_port_init()\n");
    
    /* Check initial state */
    if (timer_port_is_active(&timer)) {
        printf("FAIL: Timer should not be active after init\n");
        timer_port_cleanup(&timer);
        return -1;
    }
    printf("PASS: Timer inactive after init\n");
    
    /* Start timer with 1-second interval */
    test_callback_count = 0;
    test_start_time = time(NULL);
    result = timer_port_start(&timer, 1000000, test_timer_callback);  /* 1,000,000 us = 1 second */
    if (result != 0) {
        printf("FAIL: timer_port_start() returned %d\n", result);
        timer_port_cleanup(&timer);
        return -1;
    }
    printf("PASS: timer_port_start()\n");
    
    /* Check active state */
    if (!timer_port_is_active(&timer)) {
        printf("FAIL: Timer should be active after start\n");
        timer_port_cleanup(&timer);
        return -1;
    }
    printf("PASS: Timer active after start\n");
    
    /* Wait for a few timer callbacks */
    printf("Waiting for 5 seconds to observe timer callbacks...\n");
    sleep(5);
    
    /* Stop timer */
    result = timer_port_stop(&timer);
    if (result != 0) {
        printf("FAIL: timer_port_stop() returned %d\n", result);
        timer_port_cleanup(&timer);
        return -1;
    }
    printf("PASS: timer_port_stop()\n");
    
    /* Check inactive state */
    if (timer_port_is_active(&timer)) {
        printf("FAIL: Timer should not be active after stop\n");
        timer_port_cleanup(&timer);
        return -1;
    }
    printf("PASS: Timer inactive after stop\n");
    
    /* Verify we got some callbacks */
    printf("Callback count: %d\n", test_callback_count);
    if (test_callback_count < 3 || test_callback_count > 7) {
        printf("FAIL: Expected 4-6 callbacks in 5 seconds, got %d\n", test_callback_count);
        timer_port_cleanup(&timer);
        return -1;
    }
    printf("PASS: Received expected number of callbacks\n");
    
    /* Cleanup */
    timer_port_cleanup(&timer);
    printf("PASS: timer_port_cleanup()\n");
    
    printf("=== Basic Timer Operation Test PASSED ===\n\n");
    return 0;
}

/**
 * @brief Test high-frequency timer
 */
int test_high_frequency_timer(void)
{
    timer_port_t timer;
    int result;
    int initial_count;
    
    printf("=== Testing High-Frequency Timer ===\n");
    
    /* Initialize timer */
    result = timer_port_init(&timer);
    if (result != 0) {
        printf("FAIL: timer_port_init() returned %d\n", result);
        return -1;
    }
    
    /* Start timer with 100ms interval */
    test_callback_count = 0;
    test_start_time = time(NULL);
    result = timer_port_start(&timer, 100000, test_timer_callback);  /* 100,000 us = 100ms */
    if (result != 0) {
        printf("FAIL: timer_port_start() returned %d\n", result);
        timer_port_cleanup(&timer);
        return -1;
    }
    
    printf("Running high-frequency timer (100ms interval) for 2 seconds...\n");
    sleep(2);
    
    initial_count = test_callback_count;
    printf("Callback count after 2 seconds: %d\n", initial_count);
    
    /* Stop and cleanup */
    timer_port_stop(&timer);
    timer_port_cleanup(&timer);
    
    /* Should get roughly 20 callbacks in 2 seconds (every 100ms) */
    if (initial_count < 15 || initial_count > 25) {
        printf("FAIL: Expected ~20 callbacks in 2 seconds, got %d\n", initial_count);
        return -1;
    }
    
    printf("PASS: High-frequency timer working correctly\n");
    printf("=== High-Frequency Timer Test PASSED ===\n\n");
    return 0;
}

/**
 * @brief Test timer restart functionality
 */
int test_timer_restart(void)
{
    timer_port_t timer;
    int result;
    
    printf("=== Testing Timer Restart ===\n");
    
    /* Initialize timer */
    result = timer_port_init(&timer);
    if (result != 0) {
        printf("FAIL: timer_port_init() returned %d\n", result);
        return -1;
    }
    
    /* Start timer */
    test_callback_count = 0;
    result = timer_port_start(&timer, 500000, test_timer_callback);  /* 500ms */
    if (result != 0) {
        printf("FAIL: Initial timer_port_start() returned %d\n", result);
        timer_port_cleanup(&timer);
        return -1;
    }
    
    /* Let it run briefly */
    sleep(1);
    int first_count = test_callback_count;
    
    /* Stop timer */
    timer_port_stop(&timer);
    
    /* Try to start again (should fail if already running) */
    result = timer_port_start(&timer, 500000, test_timer_callback);
    if (result != 0) {
        printf("FAIL: Restart timer_port_start() returned %d\n", result);
        timer_port_cleanup(&timer);
        return -1;
    }
    
    /* Let it run again */
    sleep(1);
    int second_count = test_callback_count;
    
    printf("First run callbacks: %d, Total callbacks: %d\n", first_count, second_count);
    
    if (second_count <= first_count) {
        printf("FAIL: Timer restart did not continue counting\n");
        timer_port_cleanup(&timer);
        return -1;
    }
    
    /* Cleanup */
    timer_port_stop(&timer);
    timer_port_cleanup(&timer);
    
    printf("PASS: Timer restart working correctly\n");
    printf("=== Timer Restart Test PASSED ===\n\n");
    return 0;
}

/**
 * @brief Main test function
 */
int main(void)
{
    int failed_tests = 0;
    
    printf("Starting timer_port tests...\n\n");
    
    if (test_basic_timer_operation() != 0) {
        failed_tests++;
    }
    
    if (test_high_frequency_timer() != 0) {
        failed_tests++;
    }
    
    if (test_timer_restart() != 0) {
        failed_tests++;
    }
    
    printf("=== TEST SUMMARY ===\n");
    if (failed_tests == 0) {
        printf("All tests PASSED!\n");
        return 0;
    } else {
        printf("%d test(s) FAILED!\n", failed_tests);
        return 1;
    }
}