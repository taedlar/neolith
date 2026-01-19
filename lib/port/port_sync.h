/**
 * @file port_sync.h
 * @brief Platform-agnostic synchronization primitives (mutexes, events)
 * 
 * Internal use only - encapsulated within async library implementations.
 * Main thread code should use async_queue/async_worker APIs instead.
 */

#ifndef PORT_SYNC_H
#define PORT_SYNC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/**
 * Platform-agnostic mutex
 */
typedef struct {
#ifdef _WIN32
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t mutex;
#endif
} port_mutex_t;

/**
 * Platform-agnostic event (manual-reset or auto-reset)
 */
typedef struct {
#ifdef _WIN32
    HANDLE event;
#else
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool signaled;
    bool manual_reset;
#endif
} port_event_t;

/**
 * Initialize a mutex
 * @returns true on success, false on failure
 */
bool port_mutex_init(port_mutex_t* mutex);

/**
 * Destroy a mutex
 */
void port_mutex_destroy(port_mutex_t* mutex);

/**
 * Lock a mutex (blocks until acquired)
 */
void port_mutex_lock(port_mutex_t* mutex);

/**
 * Try to lock a mutex (non-blocking)
 * @returns true if lock acquired, false if already locked
 */
bool port_mutex_trylock(port_mutex_t* mutex);

/**
 * Unlock a mutex
 */
void port_mutex_unlock(port_mutex_t* mutex);

/**
 * Initialize an event
 * @param manual_reset If true, event stays signaled until reset; if false, auto-resets after one wait
 * @param initial_state If true, event starts signaled
 * @returns true on success, false on failure
 */
bool port_event_init(port_event_t* event, bool manual_reset, bool initial_state);

/**
 * Destroy an event
 */
void port_event_destroy(port_event_t* event);

/**
 * Signal an event (wake waiting threads)
 */
void port_event_set(port_event_t* event);

/**
 * Reset an event to unsignaled state
 */
void port_event_reset(port_event_t* event);

/**
 * Wait for an event to be signaled
 * @param timeout_ms Timeout in milliseconds (-1 = infinite)
 * @returns true if event signaled, false if timeout
 */
bool port_event_wait(port_event_t* event, int timeout_ms);

#endif /* PORT_SYNC_H */
