/**
 * @file timer_port.cpp
 * @brief Portable C++11 timer implementation using chrono and thread
 * 
 * This replaces platform-specific implementations (win32_timer.c, posix_timer.c, fallback_timer.c)
 * with a single portable implementation using C++11 standard library.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "timer_port.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>

/**
 * @brief Internal timer state structure
 * 
 * Uses C++11 primitives for thread-safe periodic timer execution.
 * The timer thread waits on a condition variable with a timeout,
 * providing precise timing without busy-waiting.
 */
struct timer_port_internal {
    std::thread timer_thread;
    std::mutex mutex;
    std::condition_variable cv;
    std::chrono::microseconds interval;
    timer_callback_t callback;
    std::atomic<bool> active;
    std::atomic<bool> stop_requested;
    
    timer_port_internal() : callback(nullptr), active(false), stop_requested(false) {}
};

/**
 * @brief Timer thread function
 * 
 * Executes the callback at regular intervals using condition_variable::wait_until
 * for precise timing. Stops when stop_requested is set.
 * 
 * @param internal Pointer to internal timer state
 */
static void timer_thread_func(timer_port_internal* internal) {
    auto next_tick = std::chrono::steady_clock::now() + internal->interval;
    
    while (!internal->stop_requested.load()) {
        // Wait until next tick or stop is requested
        std::cv_status status;
        {
            std::unique_lock<std::mutex> lock(internal->mutex);
            status = internal->cv.wait_until(lock, next_tick);
        }
        
        // If we were signaled (not timeout), check if we should stop
        if (internal->stop_requested.load()) {
            break;
        }
        
        // Execute callback on timeout (without holding the lock)
        if (status == std::cv_status::timeout && internal->active.load() && internal->callback) {
            internal->callback();
        }
        
        // Calculate next tick (drift correction)
        next_tick += internal->interval;
        
        // If we're running behind, reset to current time
        auto now = std::chrono::steady_clock::now();
        if (next_tick < now) {
            next_tick = now + internal->interval;
        }
    }
}

/**
 * @brief Initialize timer system
 */
extern "C" timer_error_t timer_port_init(timer_port_t* timer) {
    if (!timer) {
        return TIMER_ERR_NULL_PARAM;
    }
    
    try {
        timer->internal = new timer_port_internal();
        return TIMER_OK;
    } catch (...) {
        return TIMER_ERR_SYSTEM;
    }
}

/**
 * @brief Start the periodic timer
 */
extern "C" timer_error_t timer_port_start(timer_port_t* timer, unsigned long interval_us, timer_callback_t callback) {
    if (!timer || !timer->internal || !callback) {
        return TIMER_ERR_NULL_PARAM;
    }
    
    if (interval_us == 0) {
        return TIMER_ERR_INVALID_INTERVAL;
    }
    
    timer_port_internal* internal = static_cast<timer_port_internal*>(timer->internal);
    
    // Check if already active
    if (internal->active.load()) {
        return TIMER_ERR_ALREADY_ACTIVE;
    }
    
    try {
        // Configure timer
        internal->interval = std::chrono::microseconds(interval_us);
        internal->callback = callback;
        internal->stop_requested.store(false);
        internal->active.store(true);
        
        // Start timer thread
        internal->timer_thread = std::thread(timer_thread_func, internal);
        
        return TIMER_OK;
    } catch (...) {
        internal->active.store(false);
        return TIMER_ERR_THREAD;
    }
}

/**
 * @brief Stop the timer
 */
extern "C" timer_error_t timer_port_stop(timer_port_t* timer) {
    if (!timer || !timer->internal) {
        return TIMER_ERR_NULL_PARAM;
    }
    
    timer_port_internal* internal = static_cast<timer_port_internal*>(timer->internal);
    
    if (!internal->active.load()) {
        return TIMER_OK;  // Already stopped
    }
    
    // Signal thread to stop
    internal->active.store(false);
    internal->stop_requested.store(true);
    
    // Wake up thread if waiting
    {
        std::lock_guard<std::mutex> lock(internal->mutex);
        internal->cv.notify_all();
    }
    
    // Wait for thread to finish
    if (internal->timer_thread.joinable()) {
        internal->timer_thread.join();
    }
    
    // Clear callback
    internal->callback = nullptr;
    
    return TIMER_OK;
}

/**
 * @brief Cleanup timer resources
 */
extern "C" void timer_port_cleanup(timer_port_t* timer) {
    if (!timer || !timer->internal) {
        return;
    }
    
    timer_port_internal* internal = static_cast<timer_port_internal*>(timer->internal);
    
    // Stop timer if running
    timer_port_stop(timer);
    
    // Delete internal structure
    delete internal;
    timer->internal = nullptr;
}

/**
 * @brief Check if timer is active
 */
extern "C" int timer_port_is_active(const timer_port_t* timer) {
    if (!timer || !timer->internal) {
        return 0;
    }
    
    const timer_port_internal* internal = static_cast<const timer_port_internal*>(timer->internal);
    return internal->active.load() ? 1 : 0;
}

/**
 * @brief Convert timer error code to string
 */
extern "C" const char* timer_error_string(timer_error_t error) {
    switch (error) {
        case TIMER_OK:
            return "Success";
        case TIMER_ERR_NULL_PARAM:
            return "NULL parameter";
        case TIMER_ERR_ALREADY_ACTIVE:
            return "Timer already active";
        case TIMER_ERR_NOT_ACTIVE:
            return "Timer not active";
        case TIMER_ERR_SYSTEM:
            return "System error";
        case TIMER_ERR_THREAD:
            return "Thread creation failed";
        case TIMER_ERR_INVALID_INTERVAL:
            return "Invalid interval";
        default:
            return "Unknown error";
    }
}
