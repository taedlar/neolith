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
int timer_port_init(timer_port_t *timer)
{
    if (!timer) {
        return -1;
    }
    
    timer->timer_id = 0;
    timer->active = 0;
    memset(&timer->old_sigaction, 0, sizeof(timer->old_sigaction));
    
    return 0;
}

/**
 * @brief Start the periodic POSIX timer
 */
int timer_port_start(timer_port_t *timer, unsigned long interval_us, timer_callback_t callback)
{
    struct sigevent sev;
    struct itimerspec its;
    struct sigaction sa;
    
    if (!timer || !callback) {
        return -1;
    }
    
    if (timer->active) {
        /* Timer already running */
        return -1;
    }
    
    /* Store callback */
    g_timer_callback = callback;
    
    /* Install signal handler for SIGALRM */
    sa.sa_handler = timer_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  /* Restart interrupted system calls */
    
    if (sigaction(SIGALRM, &sa, &timer->old_sigaction) == -1) {
        return -1;
    }
    
    /* Create the timer */
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGALRM;
    sev.sigev_value.sival_ptr = timer;
    
    if (timer_create(CLOCK_REALTIME, &sev, &timer->timer_id) == -1) {
        /* Restore old signal handler */
        sigaction(SIGALRM, &timer->old_sigaction, NULL);
        return -1;
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
        return -1;
    }
    
    timer->active = 1;
    return 0;
}

/**
 * @brief Stop the POSIX timer
 */
int timer_port_stop(timer_port_t *timer)
{
    struct itimerspec its;
    
    if (!timer) {
        return -1;
    }
    
    if (!timer->active) {
        return 0;  /* Already stopped */
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
    
    return 0;
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

#endif /* HAVE_LIBRT && !_WIN32 */