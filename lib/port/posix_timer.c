/**
 * @file posix_timer.c
 * @brief POSIX librt timer implementation for high-resolution periodic timers
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(HAVE_LIBRT) && !defined(_WIN32)

#include <time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include "timer_port.h"

/* Global callback storage (could be made thread-safe with TLS if needed) */
static timer_callback_t g_timer_callback = NULL;

/**
 * @brief POSIX timer signal handler
 * 
 * This handler is called when the POSIX timer expires (SIGALRM).
 * It executes the registered callback function.
 * 
 * @param signum Signal number (should be SIGALRM)
 */
static void timer_signal_handler(int signum)
{
    (void)signum;  /* Unused parameter */
    
    if (g_timer_callback) {
        g_timer_callback();
    }
}

/**
 * @brief Initialize POSIX timer system
 */
timer_error_t timer_port_init(timer_port_t *timer)
{
    if (!timer) {
        return TIMER_ERR_NULL_PARAM;
    }
    
    timer->timer_id = 0;
    timer->active = 0;
    memset(&timer->old_sigaction, 0, sizeof(timer->old_sigaction));
    
    return TIMER_OK;
}

/**
 * @brief Start the periodic POSIX timer
 */
timer_error_t timer_port_start(timer_port_t *timer, unsigned long interval_us, timer_callback_t callback)
{
    struct sigevent sev;
    struct itimerspec its;
    struct sigaction sa;
    
    if (!timer || !callback) {
        return TIMER_ERR_NULL_PARAM;
    }
    
    if (interval_us == 0) {
        return TIMER_ERR_INVALID_INTERVAL;
    }
    
    if (timer->active) {
        /* Timer already running */
        return TIMER_ERR_ALREADY_ACTIVE;
    }
    
    /* Store callback */
    g_timer_callback = callback;
    
    /* Install signal handler for SIGALRM */
    sa.sa_handler = timer_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  /* Restart interrupted system calls */
    
    if (sigaction(SIGALRM, &sa, &timer->old_sigaction) == -1) {
        return TIMER_ERR_SYSTEM;
    }
    
    /* Create the timer */
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGALRM;
    sev.sigev_value.sival_ptr = timer;
    
    if (timer_create(CLOCK_REALTIME, &sev, &timer->timer_id) == -1) {
        /* Restore old signal handler */
        sigaction(SIGALRM, &timer->old_sigaction, NULL);
        return TIMER_ERR_SYSTEM;
    }
    
    /* Configure timer interval */
    its.it_interval.tv_sec = interval_us / 1000000;
    its.it_interval.tv_nsec = (interval_us % 1000000) * 1000;
    
    /* Set initial expiration time (same as interval) */
    its.it_value.tv_sec = its.it_interval.tv_sec;
    its.it_value.tv_nsec = its.it_interval.tv_nsec;
    
    /* Start the timer */
    if (timer_settime(timer->timer_id, 0, &its, NULL) == -1) {
        timer_delete(timer->timer_id);
        sigaction(SIGALRM, &timer->old_sigaction, NULL);
        return TIMER_ERR_SYSTEM;
    }
    
    timer->active = 1;
    return TIMER_OK;
}

/**
 * @brief Stop the POSIX timer
 */
timer_error_t timer_port_stop(timer_port_t *timer)
{
    struct itimerspec its;
    
    if (!timer) {
        return TIMER_ERR_NULL_PARAM;
    }
    
    if (!timer->active) {
        return TIMER_OK;  /* Already stopped */
    }
    
    /* Stop the timer by setting interval to zero */
    memset(&its, 0, sizeof(its));
    timer_settime(timer->timer_id, 0, &its, NULL);
    
    /* Delete the timer */
    if (timer->timer_id) {
        timer_delete(timer->timer_id);
        timer->timer_id = 0;
    }
    
    /* Restore old signal handler */
    sigaction(SIGALRM, &timer->old_sigaction, NULL);
    
    /* Clear callback */
    g_timer_callback = NULL;
    timer->active = 0;
    
    return TIMER_OK;
}

/**
 * @brief Cleanup POSIX timer resources
 */
void timer_port_cleanup(timer_port_t *timer)
{
    if (!timer) {
        return;
    }
    
    /* Stop timer if running */
    timer_port_stop(timer);
}

/**
 * @brief Check if timer is active
 */
int timer_port_is_active(const timer_port_t *timer)
{
    return (timer && timer->active) ? 1 : 0;
}

/**
 * @brief Convert timer error code to string
 */
const char *timer_error_string(timer_error_t error)
{
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

#endif /* HAVE_LIBRT && !_WIN32 */