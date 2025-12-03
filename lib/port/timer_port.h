#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file timer_port.h
 * @brief Cross-platform timer abstraction layer
 * 
 * Provides a unified interface for high-resolution periodic timers
 * across different platforms (POSIX librt and Windows waitable timers).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Timer handle type - platform specific */
#ifdef _WIN32
#include <windows.h>
typedef struct {
    HANDLE timer_handle;
    HANDLE timer_thread;
    HANDLE stop_event;
    volatile int active;
} timer_port_t;
#elif defined(HAVE_LIBRT)
#include <time.h>
#include <signal.h>
typedef struct {
    timer_t timer_id;
    struct sigaction old_sigaction;
    volatile int active;
} timer_port_t;
#else
typedef struct {
    int dummy;  /* Fallback for systems without timer support */
    volatile int active;
} timer_port_t;
#endif

/* Timer callback function type */
typedef void (*timer_callback_t)(void);

/**
 * @brief Initialize the timer system
 * @param timer Pointer to timer handle structure
 * @return 0 on success, -1 on error
 */
int timer_port_init(timer_port_t *timer);

/**
 * @brief Start a periodic timer
 * @param timer Pointer to initialized timer handle
 * @param interval_us Timer interval in microseconds
 * @param callback Function to call on each timer expiration
 * @return 0 on success, -1 on error
 */
int timer_port_start(timer_port_t *timer, unsigned long interval_us, timer_callback_t callback);

/**
 * @brief Stop the timer
 * @param timer Pointer to timer handle
 * @return 0 on success, -1 on error
 */
int timer_port_stop(timer_port_t *timer);

/**
 * @brief Cleanup timer resources
 * @param timer Pointer to timer handle
 */
void timer_port_cleanup(timer_port_t *timer);

/**
 * @brief Check if timer is currently active
 * @param timer Pointer to timer handle
 * @return 1 if active, 0 if not active
 */
int timer_port_is_active(const timer_port_t *timer);

#ifdef __cplusplus
}
#endif