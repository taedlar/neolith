/**
 * @file async_worker_pthread.c
 * @brief POSIX implementation of worker threads
 */

#ifndef _WIN32

#include "async/async_worker.h"
#include "port/port_sync.h"
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

struct async_worker_s {
    pthread_t thread;
    async_worker_proc_t proc;
    void* context;
    volatile bool should_stop;
    volatile async_worker_state_t state;
    bool thread_created;
};

/* Thread-local storage for current worker */
static __thread async_worker_t* tls_current_worker = NULL;

/* Internal thread wrapper */
static void* worker_thread_proc(void* param) {
    async_worker_t* worker = (async_worker_t*)param;
    tls_current_worker = worker;
    
    worker->state = ASYNC_WORKER_RUNNING;
    
    void* result = worker->proc(worker->context);
    
    worker->state = ASYNC_WORKER_STOPPED;
    tls_current_worker = NULL;
    
    return result;
}

async_worker_t* async_worker_create(async_worker_proc_t proc, void* context, size_t stack_size) {
    if (!proc) return NULL;
    
    async_worker_t* worker = (async_worker_t*)calloc(1, sizeof(async_worker_t));
    if (!worker) return NULL;
    
    worker->proc = proc;
    worker->context = context;
    worker->should_stop = false;
    worker->state = ASYNC_WORKER_STOPPED;
    worker->thread_created = false;
    
    /* Set stack size if requested */
    pthread_attr_t attr;
    pthread_attr_t* attr_ptr = NULL;
    
    if (stack_size > 0) {
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, stack_size);
        attr_ptr = &attr;
    }
    
    int result = pthread_create(&worker->thread, attr_ptr, worker_thread_proc, worker);
    
    if (attr_ptr) {
        pthread_attr_destroy(&attr);
    }
    
    if (result != 0) {
        free(worker);
        return NULL;
    }
    
    worker->thread_created = true;
    
    return worker;
}

void async_worker_destroy(async_worker_t* worker) {
    if (!worker) return;
    
    /* Note: pthread_t is not a handle that needs cleanup on POSIX */
    free(worker);
}

void async_worker_signal_stop(async_worker_t* worker) {
    if (worker) {
        worker->should_stop = true;
    }
}

bool async_worker_join(async_worker_t* worker, int timeout_ms) {
    if (!worker || !worker->thread_created) return false;
    
    if (timeout_ms < 0) {
        /* Infinite wait */
        int result = pthread_join(worker->thread, NULL);
        return result == 0;
    } else {
        /* Timed wait - pthread_join doesn't support timeout on all platforms */
        /* Poll state with short sleeps (not ideal but portable) */
        struct timespec sleep_time = { 0, 10000000 };  /* 10ms */
        int elapsed_ms = 0;
        
        while (worker->state != ASYNC_WORKER_STOPPED && elapsed_ms < timeout_ms) {
            nanosleep(&sleep_time, NULL);
            elapsed_ms += 10;
        }
        
        if (worker->state == ASYNC_WORKER_STOPPED) {
            pthread_join(worker->thread, NULL);
            return true;
        }
        
        return false;
    }
}

async_worker_t* async_worker_current(void) {
    return tls_current_worker;
}

bool async_worker_should_stop(async_worker_t* worker) {
    return worker ? worker->should_stop : false;
}

async_worker_state_t async_worker_get_state(const async_worker_t* worker) {
    return worker ? worker->state : ASYNC_WORKER_STOPPED;
}

#endif /* !_WIN32 */
