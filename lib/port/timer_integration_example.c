/**
 * @file timer_integration_example.c
 * @brief Example showing how to integrate the new timer_port API into backend.c
 * 
 * This file demonstrates the changes needed in src/backend.c to use the
 * new cross-platform timer implementation instead of direct librt calls.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "src/std.h"
#include "port/timer_port.h"

/* Global timer instance - would be added to backend.c */
static timer_port_t heartbeat_timer;

/* External heartbeat flag - already exists in backend.c */
extern volatile int heart_beat_flag;

/**
 * @brief Timer callback function
 * 
 * This function is called by the timer system when the heartbeat interval expires.
 * It simply sets the heart_beat_flag to indicate that heartbeat processing should occur.
 */
static void heartbeat_timer_callback(void)
{
    heart_beat_flag = 1;
    /* Optional: Add trace logging here if needed */
    /* opt_trace(TT_BACKEND|2, "Heartbeat timer fired"); */
}

/**
 * @brief Initialize the heartbeat timer system
 * 
 * This function replaces the timer_create() call in the original backend.c.
 * It should be called during backend initialization.
 * 
 * @return 0 on success, -1 on error
 */
int init_heartbeat_timer(void)
{
    if (timer_port_init(&heartbeat_timer) != 0) {
        debug_perror("timer_port_init()", NULL);
        return -1;
    }
    
    return 0;
}

/**
 * @brief Start the heartbeat timer
 * 
 * This function replaces the timer_settime() call in the original call_heart_beat().
 * It should be called to start the periodic heartbeat timer.
 * 
 * @param interval_us Heartbeat interval in microseconds
 * @return 0 on success, -1 on error
 */
int start_heartbeat_timer(unsigned long interval_us)
{
    if (timer_port_start(&heartbeat_timer, interval_us, heartbeat_timer_callback) != 0) {
        debug_perror("timer_port_start()", NULL);
        return -1;
    }
    
    return 0;
}

/**
 * @brief Stop the heartbeat timer
 * 
 * This function should be called during shutdown to stop the timer.
 * 
 * @return 0 on success, -1 on error
 */
int stop_heartbeat_timer(void)
{
    if (timer_port_stop(&heartbeat_timer) != 0) {
        debug_perror("timer_port_stop()", NULL);
        return -1;
    }
    
    return 0;
}

/**
 * @brief Cleanup heartbeat timer resources
 * 
 * This function should be called during final cleanup to free timer resources.
 */
void cleanup_heartbeat_timer(void)
{
    timer_port_cleanup(&heartbeat_timer);
}

/**
 * @brief Check if heartbeat timer is active
 * 
 * @return 1 if timer is active, 0 if not
 */
int is_heartbeat_timer_active(void)
{
    return timer_port_is_active(&heartbeat_timer);
}

/* 
 * INTEGRATION NOTES:
 * 
 * To integrate this into src/backend.c, you would:
 * 
 * 1. Replace the existing timer-related code in backend() function:
 *    
 *    OLD CODE:
 *    #ifdef HAVE_LIBRT
 *      if (-1 == timer_create (CLOCK_REALTIME, NULL, &hb_timerid)) {
 *        debug_perror ("timer_create()", NULL);
 *        return;
 *      }
 *    #else
 *      opt_warn (0, "Timer functions not available...");
 *    #endif
 *    
 *    NEW CODE:
 *    if (init_heartbeat_timer() != 0) {
 *      opt_warn(0, "Timer functions not available, heart_beat(), call_out() and reset() disabled.");
 *      return;
 *    }
 * 
 * 2. Replace timer setup in call_heart_beat() function:
 *    
 *    OLD CODE:
 *    #ifdef HAVE_LIBRT
 *      struct itimerspec itimer;
 *      itimer.it_interval.tv_sec = HEARTBEAT_INTERVAL / 1000000;
 *      itimer.it_interval.tv_nsec = (HEARTBEAT_INTERVAL % 1000000) * 1000;
 *      itimer.it_value.tv_sec = HEARTBEAT_INTERVAL / 1000000;
 *      itimer.it_value.tv_nsec = (HEARTBEAT_INTERVAL % 1000000) * 1000;
 *      if (-1 == timer_settime (hb_timerid, 0, &itimer, NULL)) {
 *        debug_perror ("timer_settime()", NULL);
 *        return;
 *      }
 *    #endif
 *    
 *    NEW CODE:
 *    if (start_heartbeat_timer(HEARTBEAT_INTERVAL) != 0) {
 *      return;
 *    }
 * 
 * 3. Remove the sigalrm_handler() function since signal handling is now
 *    encapsulated in the timer implementation.
 * 
 * 4. Add cleanup call during shutdown:
 *    cleanup_heartbeat_timer();
 * 
 * 5. Update includes:
 *    #include "port/timer_port.h"
 * 
 * 6. Remove platform-specific includes and timer_t variable:
 *    - Remove: #include <time.h>, <signal.h> (if only used for timers)
 *    - Remove: static timer_t hb_timerid = 0;
 * 
 * This approach provides a clean, cross-platform timer interface that works
 * identically on Windows (waitable timers), POSIX systems with librt, and
 * systems without native timer support (fallback implementation).
 */