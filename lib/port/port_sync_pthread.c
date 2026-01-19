/**
 * @file port_sync_pthread.c
 * @brief POSIX implementation of synchronization primitives
 */

#ifndef _WIN32

#include "port_sync.h"
#include <errno.h>
#include <time.h>
#include <sys/time.h>

/* Mutex implementation using pthread_mutex_t */

bool port_mutex_init(port_mutex_t* mutex) {
    if (!mutex) return false;
    return pthread_mutex_init(&mutex->mutex, NULL) == 0;
}

void port_mutex_destroy(port_mutex_t* mutex) {
    if (mutex) {
        pthread_mutex_destroy(&mutex->mutex);
    }
}

void port_mutex_lock(port_mutex_t* mutex) {
    if (mutex) {
        pthread_mutex_lock(&mutex->mutex);
    }
}

bool port_mutex_trylock(port_mutex_t* mutex) {
    if (!mutex) return false;
    return pthread_mutex_trylock(&mutex->mutex) == 0;
}

void port_mutex_unlock(port_mutex_t* mutex) {
    if (mutex) {
        pthread_mutex_unlock(&mutex->mutex);
    }
}

/* Event implementation using pthread_cond_t */

bool port_event_init(port_event_t* event, bool manual_reset, bool initial_state) {
    if (!event) return false;
    
    if (pthread_mutex_init(&event->mutex, NULL) != 0) {
        return false;
    }
    
    if (pthread_cond_init(&event->cond, NULL) != 0) {
        pthread_mutex_destroy(&event->mutex);
        return false;
    }
    
    event->signaled = initial_state;
    event->manual_reset = manual_reset;
    
    return true;
}

void port_event_destroy(port_event_t* event) {
    if (event) {
        pthread_cond_destroy(&event->cond);
        pthread_mutex_destroy(&event->mutex);
    }
}

void port_event_set(port_event_t* event) {
    if (!event) return;
    
    pthread_mutex_lock(&event->mutex);
    event->signaled = true;
    if (event->manual_reset) {
        pthread_cond_broadcast(&event->cond);  /* Wake all waiters */
    } else {
        pthread_cond_signal(&event->cond);     /* Wake one waiter */
    }
    pthread_mutex_unlock(&event->mutex);
}

void port_event_reset(port_event_t* event) {
    if (!event) return;
    
    pthread_mutex_lock(&event->mutex);
    event->signaled = false;
    pthread_mutex_unlock(&event->mutex);
}

bool port_event_wait(port_event_t* event, int timeout_ms) {
    if (!event) return false;
    
    pthread_mutex_lock(&event->mutex);
    
    bool result = false;
    
    if (event->signaled) {
        result = true;
        if (!event->manual_reset) {
            event->signaled = false;  /* Auto-reset */
        }
    } else if (timeout_ms == 0) {
        /* Non-blocking check */
        result = false;
    } else if (timeout_ms < 0) {
        /* Infinite wait */
        while (!event->signaled) {
            pthread_cond_wait(&event->cond, &event->mutex);
        }
        result = true;
        if (!event->manual_reset) {
            event->signaled = false;
        }
    } else {
        /* Timed wait */
        struct timespec ts;
        struct timeval now;
        gettimeofday(&now, NULL);
        
        ts.tv_sec = now.tv_sec + (timeout_ms / 1000);
        ts.tv_nsec = (now.tv_usec * 1000) + ((timeout_ms % 1000) * 1000000);
        
        /* Handle nanosecond overflow */
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
        
        while (!event->signaled) {
            int wait_result = pthread_cond_timedwait(&event->cond, &event->mutex, &ts);
            if (wait_result == ETIMEDOUT) {
                break;
            }
        }
        
        if (event->signaled) {
            result = true;
            if (!event->manual_reset) {
                event->signaled = false;
            }
        }
    }
    
    pthread_mutex_unlock(&event->mutex);
    
    return result;
}

#endif /* !_WIN32 */
