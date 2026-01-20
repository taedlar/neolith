/**
 * @file async_worker_win32.c
 * @brief Windows implementation of worker threads
 */

#ifdef _WIN32

#include "async/async_worker.h"
#include "port/debug.h"
#include "port/port_sync.h"
#include <windows.h>
#include <stdlib.h>

struct async_worker_s {
    HANDLE thread;
    DWORD thread_id;
    async_worker_proc_t proc;
    void* context;
    async_worker_state_t state;
    port_event_t stop_event;
};

/* Thread-local storage for current worker */
static __declspec(thread) async_worker_t* tls_current_worker = NULL;

/* Internal thread wrapper */
static DWORD WINAPI worker_thread_proc(LPVOID param) {
    async_worker_t* worker = (async_worker_t*)param;
    tls_current_worker = worker;
    
    worker->state = ASYNC_WORKER_RUNNING;
    
    void* result = worker->proc(worker->context);
    
    worker->state = ASYNC_WORKER_STOPPED;
    tls_current_worker = NULL;
    
    return (DWORD)(uintptr_t)result;
}

async_worker_t* async_worker_create(async_worker_proc_t proc, void* context, size_t stack_size) {
    if (!proc) return NULL;
    
    async_worker_t* worker = (async_worker_t*)calloc(1, sizeof(async_worker_t));
    if (!worker) return NULL;
    
    worker->proc = proc;
    worker->context = context;
    worker->state = ASYNC_WORKER_STOPPED;
    
    if (!port_event_init(&worker->stop_event, true, false)) {
        free(worker);
        return NULL;
    }
    
    worker->thread = CreateThread(
        NULL,                           /* default security */
        stack_size,                     /* stack size (0 = default) */
        worker_thread_proc,
        worker,
        0,                              /* run immediately */
        &worker->thread_id
    );
    
    if (!worker->thread) {
        free(worker);
        return NULL;
    }
    
    return worker;
}

void async_worker_destroy(async_worker_t* worker) {
    if (!worker) return;
    
    if (worker->thread) {
        CloseHandle(worker->thread);
    }
    port_event_destroy(&worker->stop_event);
    free(worker);
}

void async_worker_signal_stop(async_worker_t* worker) {
    if (worker) {
        port_event_set(&worker->stop_event);
    }
}

bool async_worker_join(async_worker_t* worker, int timeout_ms) {
    if (!worker || !worker->thread) return false;
    
    DWORD timeout = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
    DWORD result = WaitForSingleObject(worker->thread, timeout);
    
    return result == WAIT_OBJECT_0;
}

async_worker_t* async_worker_current(void) {
    return tls_current_worker;
}

bool async_worker_should_stop(async_worker_t* worker) {
    if (!worker) return false;
    return port_event_wait(&worker->stop_event, 0);
}

async_worker_state_t async_worker_get_state(const async_worker_t* worker) {
    return worker ? worker->state : ASYNC_WORKER_STOPPED;
}

port_event_t* async_worker_get_stop_event(async_worker_t* worker) {
    return worker ? &worker->stop_event : NULL;
}

#endif /* _WIN32 */
