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
                if (timer->active && timer->callback) {
                    timer->callback();
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
timer_error_t timer_port_init(timer_port_t *timer)
{
    if (!timer) {
        return TIMER_ERR_NULL_PARAM;
    }
    
    /* Initialize all handles to invalid/null */
    timer->timer_handle = NULL;
    timer->timer_thread = NULL;
    timer->stop_event = NULL;
    timer->callback = NULL;
    timer->active = 0;
    
    /* Create manual-reset stop event */
    timer->stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!timer->stop_event) {
        return TIMER_ERR_SYSTEM;
    }
    
    /* Create waitable timer */
    timer->timer_handle = CreateWaitableTimer(NULL, FALSE, NULL);
    if (!timer->timer_handle) {
        CloseHandle(timer->stop_event);
        timer->stop_event = NULL;
        return TIMER_ERR_SYSTEM;
    }
    
    return TIMER_OK;
}

/**
 * @brief Start the periodic timer
 */
timer_error_t timer_port_start(timer_port_t *timer, unsigned long interval_us, timer_callback_t callback)
{
    LARGE_INTEGER due_time;
    LONG period_ms;
    
    if (!timer || !callback) {
        return TIMER_ERR_NULL_PARAM;
    }
    
    if (!timer->timer_handle || !timer->stop_event) {
        return TIMER_ERR_SYSTEM;
    }
    
    if (timer->active) {
        /* Timer already running */
        return TIMER_ERR_ALREADY_ACTIVE;
    }
    
    if (interval_us == 0) {
        return TIMER_ERR_INVALID_INTERVAL;
    }
    
    /* Store callback in timer structure */
    timer->callback = callback;
    
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
        return TIMER_ERR_SYSTEM;
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
        return TIMER_ERR_THREAD;
    }
    
    return TIMER_OK;
}

/**
 * @brief Stop the timer
 */
timer_error_t timer_port_stop(timer_port_t *timer)
{
    if (!timer) {
        return TIMER_ERR_NULL_PARAM;
    }
    
    if (!timer->active) {
        return TIMER_OK;  /* Already stopped */
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
    timer->callback = NULL;
    
    return TIMER_OK;
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

#endif /* _WIN32 */