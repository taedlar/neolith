/**
 * @file async_queue.c
 * @brief Thread-safe message queue implementation using circular buffer
 */

#include "async/async_queue.h"
#include "port/sync.h"
#include <stdlib.h>
#include <string.h>

/**
 * Queue implementation using circular buffer
 */
struct async_queue_s {
    platform_mutex_t mutex;        /* Protects queue state */
    platform_event_t not_full;     /* Signaled when space available (for BLOCK_WRITER) */
    platform_event_t not_empty;    /* Signaled when data available (for SIGNAL_ON_DATA) */
    
    void* buffer;              /* Circular buffer storage */
    size_t capacity;           /* Maximum number of messages */
    size_t max_msg_size;       /* Maximum message size */
    size_t msg_slot_size;      /* Actual slot size (max_msg_size + sizeof(size_t)) */
    
    size_t head;               /* Write position */
    size_t tail;               /* Read position */
    size_t count;              /* Current message count */
    
    async_queue_flags_t flags;
    
    /* Statistics */
    uint64_t enqueue_count;
    uint64_t dequeue_count;
    uint64_t dropped_count;
};

/* Internal helper: get slot pointer */
static inline void* get_slot(async_queue_t* queue, size_t index) {
    return (char*)queue->buffer + (index * queue->msg_slot_size);
}

async_queue_t* async_queue_create(size_t capacity, size_t max_msg_size, async_queue_flags_t flags) {
    if (capacity == 0 || max_msg_size == 0) {
        return NULL;
    }
    
    async_queue_t* queue = (async_queue_t*)calloc(1, sizeof(async_queue_t));
    if (!queue) {
        return NULL;
    }
    
    /* Initialize mutex */
    if (!platform_mutex_init(&queue->mutex)) {
        free(queue);
        return NULL;
    }
    
    /* Initialize events if needed */
    if (flags & ASYNC_QUEUE_BLOCK_WRITER) {
        if (!platform_event_init(&queue->not_full, false, true)) {
            platform_mutex_destroy(&queue->mutex);
            free(queue);
            return NULL;
        }
    }
    
    if (flags & ASYNC_QUEUE_SIGNAL_ON_DATA) {
        if (!platform_event_init(&queue->not_empty, false, false)) {
            if (flags & ASYNC_QUEUE_BLOCK_WRITER) {
                platform_event_destroy(&queue->not_full);
            }
            platform_mutex_destroy(&queue->mutex);
            free(queue);
            return NULL;
        }
    }
    
    /* Allocate circular buffer (each slot: size_t for length + message data) */
    queue->msg_slot_size = sizeof(size_t) + max_msg_size;
    queue->buffer = calloc(capacity, queue->msg_slot_size);
    if (!queue->buffer) {
        if (flags & ASYNC_QUEUE_SIGNAL_ON_DATA) {
            platform_event_destroy(&queue->not_empty);
        }
        if (flags & ASYNC_QUEUE_BLOCK_WRITER) {
            platform_event_destroy(&queue->not_full);
        }
        platform_mutex_destroy(&queue->mutex);
        free(queue);
        return NULL;
    }
    
    queue->capacity = capacity;
    queue->max_msg_size = max_msg_size;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->flags = flags;
    queue->enqueue_count = 0;
    queue->dequeue_count = 0;
    queue->dropped_count = 0;
    
    return queue;
}

void async_queue_destroy(async_queue_t* queue) {
    if (!queue) return;
    
    if (queue->buffer) {
        free(queue->buffer);
    }
    
    if (queue->flags & ASYNC_QUEUE_SIGNAL_ON_DATA) {
        platform_event_destroy(&queue->not_empty);
    }
    
    if (queue->flags & ASYNC_QUEUE_BLOCK_WRITER) {
        platform_event_destroy(&queue->not_full);
    }
    
    platform_mutex_destroy(&queue->mutex);
    free(queue);
}

bool async_queue_enqueue(async_queue_t* queue, const void* data, size_t size) {
    if (!queue || !data || size == 0 || size > queue->max_msg_size) {
        return false;
    }
    
    platform_mutex_lock(&queue->mutex);
    
    /* Check if queue is full */
    while (queue->count >= queue->capacity) {
        if (queue->flags & ASYNC_QUEUE_DROP_OLDEST) {
            /* Drop oldest message */
            queue->tail = (queue->tail + 1) % queue->capacity;
            queue->count--;
            queue->dropped_count++;
        } else if (queue->flags & ASYNC_QUEUE_BLOCK_WRITER) {
            /* Wait for space (release mutex while waiting) */
            platform_mutex_unlock(&queue->mutex);
            platform_event_wait(&queue->not_full, -1);
            platform_mutex_lock(&queue->mutex);
            /* Re-check after waking up */
            continue;
        } else {
            /* Queue full, fail immediately */
            platform_mutex_unlock(&queue->mutex);
            return false;
        }
    }
    
    /* Write message to slot (length + data) */
    void* slot = get_slot(queue, queue->head);
    *(size_t*)slot = size;
    memcpy((char*)slot + sizeof(size_t), data, size);
    
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count++;
    queue->enqueue_count++;
    
    /* Signal not_empty if configured */
    if (queue->flags & ASYNC_QUEUE_SIGNAL_ON_DATA) {
        platform_event_set(&queue->not_empty);
    }
    
    platform_mutex_unlock(&queue->mutex);
    
    return true;
}

bool async_queue_dequeue(async_queue_t* queue, void* buffer, size_t buffer_size, size_t* out_size) {
    if (!queue || !buffer) {
        return false;
    }
    
    platform_mutex_lock(&queue->mutex);
    
    if (queue->count == 0) {
        platform_mutex_unlock(&queue->mutex);
        return false;
    }
    
    /* Read message from slot */
    void* slot = get_slot(queue, queue->tail);
    size_t msg_size = *(size_t*)slot;
    
    if (msg_size > buffer_size) {
        /* Buffer too small */
        platform_mutex_unlock(&queue->mutex);
        return false;
    }
    
    memcpy(buffer, (char*)slot + sizeof(size_t), msg_size);
    
    if (out_size) {
        *out_size = msg_size;
    }
    
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count--;
    queue->dequeue_count++;
    
    /* Signal not_full if configured */
    if (queue->flags & ASYNC_QUEUE_BLOCK_WRITER) {
        platform_event_set(&queue->not_full);
    }
    
    platform_mutex_unlock(&queue->mutex);
    
    return true;
}

bool async_queue_is_empty(const async_queue_t* queue) {
    if (!queue) return true;
    
    platform_mutex_lock((platform_mutex_t*)&queue->mutex);
    bool empty = (queue->count == 0);
    platform_mutex_unlock((platform_mutex_t*)&queue->mutex);
    
    return empty;
}

bool async_queue_is_full(const async_queue_t* queue) {
    if (!queue) return false;
    
    platform_mutex_lock((platform_mutex_t*)&queue->mutex);
    bool full = (queue->count >= queue->capacity);
    platform_mutex_unlock((platform_mutex_t*)&queue->mutex);
    
    return full;
}

void async_queue_clear(async_queue_t* queue) {
    if (!queue) return;
    
    platform_mutex_lock(&queue->mutex);
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    platform_mutex_unlock(&queue->mutex);
}

void async_queue_get_stats(const async_queue_t* queue, async_queue_stats_t* stats) {
    if (!queue || !stats) return;
    
    platform_mutex_lock((platform_mutex_t*)&queue->mutex);
    stats->capacity = queue->capacity;
    stats->current_size = queue->count;
    stats->max_msg_size = queue->max_msg_size;
    stats->enqueue_count = queue->enqueue_count;
    stats->dequeue_count = queue->dequeue_count;
    stats->dropped_count = queue->dropped_count;
    platform_mutex_unlock((platform_mutex_t*)&queue->mutex);
}
