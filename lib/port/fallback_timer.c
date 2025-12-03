/**
 * @file fallback_timer.c
 * @brief Fallback timer implementation for systems without native timer support
 * 
 * This implementation provides a basic polling-based timer using sleep functions.
 * It's not suitable for high-precision timing but provides basic functionality
 * for systems that don't have librt or Windows waitable timers.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if !defined(HAVE_LIBRT) && !defined(_WIN32)

#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "timer_port.h"

/* Fallback timer structure */
typedef struct {
    pthread_t thread;
    pthread_mutex_t mutex;
    int active;
    unsigned long interval_us;
    timer_callback_t callback;
} fallback_timer_data_t;

static fallback_timer_data_t g_fallback_timer = {0};

/**
 * @brief Fallback timer thread procedure
 * 
 * This thread implements a simple polling loop with sleep between iterations.
 * Not suitable for high-precision timing but provides basic functionality.
 * 
 * @param arg Unused parameter
 * @return NULL
 */
static void *fallback_timer_thread(void *arg)
{
    (void)arg;  /* Unused parameter */
    
    struct timespec sleep_time;
    timer_callback_t callback;
    unsigned long interval;
    
    while (1) {
        pthread_mutex_lock(&g_fallback_timer.mutex);
        
        if (!g_fallback_timer.active) {
            pthread_mutex_unlock(&g_fallback_timer.mutex);
            break;
        }
        
        callback = g_fallback_timer.callback;
        interval = g_fallback_timer.interval_us;
        
        pthread_mutex_unlock(&g_fallback_timer.mutex);
        
        /* Execute callback if set */
        if (callback) {
            callback();
        }
        
        /* Sleep for the specified interval */
        sleep_time.tv_sec = interval / 1000000;
        sleep_time.tv_nsec = (interval % 1000000) * 1000;
        nanosleep(&sleep_time, NULL);
    }
    
    return NULL;
}

/**
 * @brief Initialize fallback timer system
 */
int timer_port_init(timer_port_t *timer)
{
    if (!timer) {
        return -1;
    }
    
    timer->active = 0;
    
    /* Initialize global fallback timer data */
    if (pthread_mutex_init(&g_fallback_timer.mutex, NULL) != 0) {
        return -1;
    }
    
    g_fallback_timer.active = 0;
    g_fallback_timer.interval_us = 0;
    g_fallback_timer.callback = NULL;
    
    return 0;
}

/**
 * @brief Start the fallback timer
 */
int timer_port_start(timer_port_t *timer, unsigned long interval_us, timer_callback_t callback)
{
    if (!timer || !callback) {
        return -1;
    }
    
    pthread_mutex_lock(&g_fallback_timer.mutex);
    
    if (g_fallback_timer.active) {
        /* Timer already running */
        pthread_mutex_unlock(&g_fallback_timer.mutex);
        return -1;
    }
    
    /* Configure timer */
    g_fallback_timer.interval_us = interval_us;
    g_fallback_timer.callback = callback;
    g_fallback_timer.active = 1;
    
    /* Create timer thread */
    if (pthread_create(&g_fallback_timer.thread, NULL, fallback_timer_thread, NULL) != 0) {
        g_fallback_timer.active = 0;
        pthread_mutex_unlock(&g_fallback_timer.mutex);
        return -1;
    }
    
    timer->active = 1;
    pthread_mutex_unlock(&g_fallback_timer.mutex);
    
    return 0;
}

/**
 * @brief Stop the fallback timer
 */
int timer_port_stop(timer_port_t *timer)
{
    if (!timer) {
        return -1;
    }
    
    pthread_mutex_lock(&g_fallback_timer.mutex);
    
    if (!g_fallback_timer.active) {
        pthread_mutex_unlock(&g_fallback_timer.mutex);
        return 0;  /* Already stopped */
    }
    
    /* Signal thread to stop */
    g_fallback_timer.active = 0;
    timer->active = 0;
    
    pthread_mutex_unlock(&g_fallback_timer.mutex);
    
    /* Wait for thread to finish */
    pthread_join(g_fallback_timer.thread, NULL);
    
    return 0;
}

/**
 * @brief Cleanup fallback timer resources
 */
void timer_port_cleanup(timer_port_t *timer)
{
    if (!timer) {
        return;
    }
    
    /* Stop timer if running */
    timer_port_stop(timer);
    
    /* Cleanup mutex */
    pthread_mutex_destroy(&g_fallback_timer.mutex);
}

/**
 * @brief Check if timer is active
 */
int timer_port_is_active(const timer_port_t *timer)
{
    return (timer && timer->active) ? 1 : 0;
}

#endif /* !HAVE_LIBRT && !_WIN32 */