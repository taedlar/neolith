/**
 * @file port_sync_win32.c
 * @brief Windows implementation of synchronization primitives
 */

#ifdef _WIN32

#include "port_sync.h"
#include <windows.h>

/* Mutex implementation using CRITICAL_SECTION */

bool port_mutex_init(port_mutex_t* mutex) {
    if (!mutex) return false;
    InitializeCriticalSection(&mutex->cs);
    return true;
}

void port_mutex_destroy(port_mutex_t* mutex) {
    if (mutex) {
        DeleteCriticalSection(&mutex->cs);
    }
}

void port_mutex_lock(port_mutex_t* mutex) {
    if (mutex) {
        EnterCriticalSection(&mutex->cs);
    }
}

bool port_mutex_trylock(port_mutex_t* mutex) {
    if (!mutex) return false;
    return TryEnterCriticalSection(&mutex->cs) != 0;
}

void port_mutex_unlock(port_mutex_t* mutex) {
    if (mutex) {
        LeaveCriticalSection(&mutex->cs);
    }
}

/* Event implementation using Windows Events */

bool port_event_init(port_event_t* event, bool manual_reset, bool initial_state) {
    if (!event) return false;
    
    event->event = CreateEvent(
        NULL,                   /* no security attributes */
        manual_reset ? TRUE : FALSE,
        initial_state ? TRUE : FALSE,
        NULL                    /* unnamed event */
    );
    
    return event->event != NULL;
}

void port_event_destroy(port_event_t* event) {
    if (event && event->event) {
        CloseHandle(event->event);
        event->event = NULL;
    }
}

void port_event_set(port_event_t* event) {
    if (event && event->event) {
        SetEvent(event->event);
    }
}

void port_event_reset(port_event_t* event) {
    if (event && event->event) {
        ResetEvent(event->event);
    }
}

bool port_event_wait(port_event_t* event, int timeout_ms) {
    if (!event || !event->event) return false;
    
    DWORD timeout = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
    DWORD result = WaitForSingleObject(event->event, timeout);
    
    return result == WAIT_OBJECT_0;
}

#endif /* _WIN32 */
