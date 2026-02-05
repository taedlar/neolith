/**
 * @file sync.cpp
 * @brief C++11 implementation of platform-agnostic synchronization primitives
 */

#include "sync.h"
#include <mutex>
#include <new>

#ifndef _WIN32
#include <condition_variable>
#include <chrono>
#else
#include <windows.h>
#endif

/* Internal C++ wrapper types */
struct MutexImpl {
    std::mutex mtx;
};

#ifdef _WIN32
/* Windows: Use native event for WaitForMultipleObjects compatibility */
struct EventImpl {
    HANDLE event;
};
#else
/* POSIX: Use C++11 condition_variable */
struct EventImpl {
    std::mutex mtx;
    std::condition_variable cv;
    bool signaled;
    bool manual_reset;
};
#endif

/* Static assertions to ensure opaque storage is large enough */
static_assert(sizeof(MutexImpl) <= sizeof(platform_mutex_t), 
              "platform_mutex_t storage too small for std::mutex");
static_assert(sizeof(EventImpl) <= sizeof(platform_event_t),
              "platform_event_t storage too small for EventImpl");

/* Helper to get MutexImpl pointer from opaque storage */
static inline MutexImpl* get_mutex(platform_mutex_t* mutex) {
    return reinterpret_cast<MutexImpl*>(mutex);
}

/* Helper to get EventImpl pointer from opaque storage */
static inline EventImpl* get_event(platform_event_t* event) {
    return reinterpret_cast<EventImpl*>(event);
}

extern "C" {

/* Mutex API */

bool platform_mutex_init(platform_mutex_t* mutex) {
    if (!mutex) return false;
    
    try {
        new (mutex) MutexImpl{};
        return true;
    } catch (...) {
        return false;
    }
}

void platform_mutex_destroy(platform_mutex_t* mutex) {
    if (mutex) {
        get_mutex(mutex)->~MutexImpl();
    }
}

void platform_mutex_lock(platform_mutex_t* mutex) {
    if (mutex) {
        get_mutex(mutex)->mtx.lock();
    }
}

bool platform_mutex_trylock(platform_mutex_t* mutex) {
    if (!mutex) return false;
    return get_mutex(mutex)->mtx.try_lock();
}

void platform_mutex_unlock(platform_mutex_t* mutex) {
    if (mutex) {
        get_mutex(mutex)->mtx.unlock();
    }
}

/* Event API */

bool platform_event_init(platform_event_t* event, bool manual_reset, bool initial_state) {
    if (!event) return false;
    
    try {
#ifdef _WIN32
        HANDLE h = CreateEvent(
            NULL,
            manual_reset ? TRUE : FALSE,
            initial_state ? TRUE : FALSE,
            NULL
        );
        if (!h) return false;
        
        new (event) EventImpl{ h };
        return true;
#else
        new (event) EventImpl{
            std::mutex{},
            std::condition_variable{},
            initial_state,
            manual_reset
        };
        return true;
#endif
    } catch (...) {
        return false;
    }
}

void platform_event_destroy(platform_event_t* event) {
    if (event) {
#ifdef _WIN32
        EventImpl* impl = get_event(event);
        if (impl->event) {
            CloseHandle(impl->event);
        }
#endif
        get_event(event)->~EventImpl();
    }
}

void platform_event_set(platform_event_t* event) {
    if (!event) return;
    
#ifdef _WIN32
    EventImpl* impl = get_event(event);
    if (impl->event) {
        SetEvent(impl->event);
    }
#else
    EventImpl* impl = get_event(event);
    std::lock_guard<std::mutex> lock(impl->mtx);
    impl->signaled = true;
    
    if (impl->manual_reset) {
        impl->cv.notify_all();  /* Wake all waiters for manual-reset */
    } else {
        impl->cv.notify_one();  /* Wake one waiter for auto-reset */
    }
#endif
}

void platform_event_reset(platform_event_t* event) {
    if (!event) return;
    
#ifdef _WIN32
    EventImpl* impl = get_event(event);
    if (impl->event) {
        ResetEvent(impl->event);
    }
#else
    EventImpl* impl = get_event(event);
    std::lock_guard<std::mutex> lock(impl->mtx);
    impl->signaled = false;
#endif
}

bool platform_event_wait(platform_event_t* event, int timeout_ms) {
    if (!event) return false;
    
    EventImpl* impl = get_event(event);
    
#ifdef _WIN32
    if (!impl->event) return false;
    
    DWORD timeout = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
    DWORD result = WaitForSingleObject(impl->event, timeout);
    
    return result == WAIT_OBJECT_0;
#else
    std::unique_lock<std::mutex> lock(impl->mtx);
    
    bool result;
    
    if (impl->signaled) {
        /* Already signaled */
        result = true;
        if (!impl->manual_reset) {
            impl->signaled = false;  /* Auto-reset */
        }
    } else if (timeout_ms == 0) {
        /* Non-blocking check */
        result = false;
    } else if (timeout_ms < 0) {
        /* Infinite wait */
        impl->cv.wait(lock, [impl] { return impl->signaled; });
        result = true;
        if (!impl->manual_reset) {
            impl->signaled = false;  /* Auto-reset */
        }
    } else {
        /* Timed wait with proper spurious wakeup handling */
        result = impl->cv.wait_for(
            lock,
            std::chrono::milliseconds(timeout_ms),
            [impl] { return impl->signaled; }
        );
        
        if (result && !impl->manual_reset) {
            impl->signaled = false;  /* Auto-reset */
        }
    }
    
    return result;
#endif
}

#ifdef _WIN32
void* platform_event_get_native_handle(platform_event_t* event) {
    if (!event) return NULL;
    return get_event(event)->event;
}
#endif

} /* extern "C" */
