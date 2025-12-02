/**
 * @file win32_timer.c
 * @brief Windows waitable timer implementation for high-resolution periodic timers
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _WIN32

#include <windows.h>
#include <process.h>
#include "timer_port.h"

/* Thread-local storage for callback function */
static __declspec(thread) timer_callback_t g_timer_callback = NULL;

/**
 * @brief Timer thread procedure
 * 
 * This thread waits on the waitable timer and executes the callback
 * function when the timer signals. It runs until the stop event is set.
 * 
 * @param lpParameter Pointer to timer_port_t structure
 * @return Thread exit code (always 0)
 */
static unsigned __stdcall timer_thread_proc(void *lpParameter)
{
    timer_port_t *timer = (timer_port_t *)lpParameter;
    HANDLE wait_handles[2] = { timer->timer_handle, timer->stop_event };
    DWORD wait_result;
    
    while (timer->active) {
        wait_result = WaitForMultipleObjects(2, wait_handles, FALSE, INFINITE);
        
        switch (wait_result) {
            case WAIT_OBJECT_0:  /* Timer signaled */
                if (timer->active && g_timer_callback) {
                    g_timer_callback();
                }
                break;
                
            case WAIT_OBJECT_0 + 1:  /* Stop event signaled */
                timer->active = 0;
                break;
                
            case WAIT_FAILED:
                /* Log error and exit thread */
                timer->active = 0;
                break;
                
            default:
                /* Unexpected return value */
                break;
        }
    }
    
    return 0;
}

/**
 * @brief Initialize Windows timer system
 */
int timer_port_init(timer_port_t *timer)
{
    if (!timer) {
        return -1;
    }
    
    /* Initialize all handles to invalid/null */
    timer->timer_handle = NULL;
    timer->timer_thread = NULL;
    timer->stop_event = NULL;
    timer->active = 0;
    
    /* Create manual-reset stop event */
    timer->stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!timer->stop_event) {
        return -1;
    }
    
    /* Create waitable timer */
    timer->timer_handle = CreateWaitableTimer(NULL, FALSE, NULL);
    if (!timer->timer_handle) {
        CloseHandle(timer->stop_event);
        timer->stop_event = NULL;
        return -1;
    }
    
    return 0;
}

/**
 * @brief Start the periodic timer
 */
int timer_port_start(timer_port_t *timer, unsigned long interval_us, timer_callback_t callback)
{
    LARGE_INTEGER due_time;
    LONG period_ms;
    
    if (!timer || !callback || !timer->timer_handle || !timer->stop_event) {
        return -1;
    }
    
    if (timer->active) {
        /* Timer already running */
        return -1;
    }
    
    /* Store callback in thread-local storage */
    g_timer_callback = callback;
    
    /* Convert microseconds to milliseconds for period */
    period_ms = (LONG)(interval_us / 1000);
    if (period_ms < 1) {
        period_ms = 1;  /* Minimum 1ms resolution */
    }
    
    /* Set initial due time (negative value = relative time) */
    /* 100-nanosecond intervals, so multiply by 10 to convert from microseconds */
    due_time.QuadPart = -((LONGLONG)interval_us * 10);
    
    /* Reset the stop event */
    ResetEvent(timer->stop_event);
    
    /* Set the waitable timer */
    if (!SetWaitableTimer(timer->timer_handle, &due_time, period_ms, NULL, NULL, FALSE)) {
        return -1;
    }
    
    /* Mark timer as active before starting thread */
    timer->active = 1;
    
    /* Create timer thread */
    timer->timer_thread = (HANDLE)_beginthreadex(
        NULL,               /* Security attributes */
        0,                  /* Stack size (default) */
        timer_thread_proc,  /* Thread procedure */
        timer,              /* Thread parameter */
        0,                  /* Creation flags */
        NULL                /* Thread ID (not needed) */
    );
    
    if (!timer->timer_thread) {
        timer->active = 0;
        CancelWaitableTimer(timer->timer_handle);
        return -1;
    }
    
    return 0;
}

/**
 * @brief Stop the timer
 */
int timer_port_stop(timer_port_t *timer)
{
    if (!timer) {
        return -1;
    }
    
    if (!timer->active) {
        return 0;  /* Already stopped */
    }
    
    /* Signal the timer thread to stop */
    timer->active = 0;
    if (timer->stop_event) {
        SetEvent(timer->stop_event);
    }
    
    /* Cancel the waitable timer */
    if (timer->timer_handle) {
        CancelWaitableTimer(timer->timer_handle);
    }
    
    /* Wait for timer thread to finish (with timeout) */
    if (timer->timer_thread) {
        WaitForSingleObject(timer->timer_thread, 5000);  /* 5 second timeout */
        CloseHandle(timer->timer_thread);
        timer->timer_thread = NULL;
    }
    
    /* Clear callback */
    g_timer_callback = NULL;
    
    return 0;
}

/**
 * @brief Cleanup timer resources
 */
void timer_port_cleanup(timer_port_t *timer)
{
    if (!timer) {
        return;
    }
    
    /* Stop timer if running */
    timer_port_stop(timer);
    
    /* Close handles */
    if (timer->timer_handle) {
        CloseHandle(timer->timer_handle);
        timer->timer_handle = NULL;
    }
    
    if (timer->stop_event) {
        CloseHandle(timer->stop_event);
        timer->stop_event = NULL;
    }
    
    timer->active = 0;
}

/**
 * @brief Check if timer is active
 */
int timer_port_is_active(const timer_port_t *timer)
{
    return (timer && timer->active) ? 1 : 0;
}

#endif /* _WIN32 */