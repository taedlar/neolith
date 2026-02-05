/**
 * @file sync.h
 * @brief Platform-agnostic synchronization primitives (mutexes, events)
 * 
 * C++11-based implementation providing C-compatible API.
 * Internal use only - encapsulated within async library implementations.
 * Main thread code should use async_queue/async_worker APIs instead.
 */

#ifndef PORT_SYNC_H
#define PORT_SYNC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Platform-agnostic mutex
 * Opaque storage - actual C++ object constructed via placement new
 */
typedef struct platform_mutex_s {
    /* Opaque storage sized for std::mutex (typically 40-80 bytes) */
    uint64_t _opaque[10];
} platform_mutex_t;

/**
 * Platform-agnostic event
 * Opaque storage - actual C++ objects constructed via placement new
 */
typedef struct platform_event_s {
    /* Opaque storage sized for std::mutex + std::condition_variable + flags */
    uint64_t _opaque[20];
} platform_event_t;

/**
 * Initialize a mutex
 * @returns true on success, false on failure
 */
bool platform_mutex_init(platform_mutex_t* mutex);

/**
 * Destroy a mutex
 */
void platform_mutex_destroy(platform_mutex_t* mutex);

/**
 * Lock a mutex (blocks until acquired)
 */
void platform_mutex_lock(platform_mutex_t* mutex);

/**
 * Try to lock a mutex (non-blocking)
 * @returns true if lock acquired, false if already locked
 */
bool platform_mutex_trylock(platform_mutex_t* mutex);

/**
 * Unlock a mutex
 */
void platform_mutex_unlock(platform_mutex_t* mutex);

/**
 * Initialize an event
 * @param manual_reset If true, event stays signaled until reset; if false, auto-resets after one wait
 * @param initial_state If true, event starts signaled
 * @returns true on success, false on failure
 */
bool platform_event_init(platform_event_t* event, bool manual_reset, bool initial_state);

/**
 * Destroy an event
 */
void platform_event_destroy(platform_event_t* event);

/**
 * Signal an event (wake waiting threads)
 */
void platform_event_set(platform_event_t* event);

/**
 * Reset an event to unsignaled state
 */
void platform_event_reset(platform_event_t* event);

/**
 * Wait for an event to be signaled
 * @param timeout_ms Timeout in milliseconds (-1 = infinite)
 * @returns true if event signaled, false if timeout
 */
bool platform_event_wait(platform_event_t* event, int timeout_ms);

#ifdef _WIN32
/**
 * Get native Windows HANDLE for event (for WaitForMultipleObjects usage)
 * @param event Event to query
 * @returns Windows HANDLE, or NULL if invalid
 */
void* platform_event_get_native_handle(platform_event_t* event);
#endif

#ifdef __cplusplus
}
#endif

#endif /* PORT_SYNC_H */
