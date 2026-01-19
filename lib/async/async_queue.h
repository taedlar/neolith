/**
 * @file async_queue.h
 * @brief Thread-safe FIFO message queue for async communication
 */

#ifndef ASYNC_QUEUE_H
#define ASYNC_QUEUE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct async_queue_s async_queue_t;

/**
 * Queue behavior flags
 */
typedef enum {
    ASYNC_QUEUE_DROP_OLDEST = 0x01,    /**< Drop oldest message when full */
    ASYNC_QUEUE_BLOCK_WRITER = 0x02,   /**< Block enqueue until space available */
    ASYNC_QUEUE_SIGNAL_ON_DATA = 0x04  /**< Signal event when data enqueued */
} async_queue_flags_t;

/**
 * Queue statistics
 */
typedef struct {
    size_t capacity;           /**< Maximum queue capacity */
    size_t current_size;       /**< Current number of messages */
    size_t max_msg_size;       /**< Maximum message size */
    uint64_t enqueue_count;    /**< Total messages enqueued */
    uint64_t dequeue_count;    /**< Total messages dequeued */
    uint64_t dropped_count;    /**< Messages dropped (when DROP_OLDEST) */
} async_queue_stats_t;

/**
 * Create a new message queue
 * 
 * @param capacity Maximum number of messages (should be power of 2 for best performance)
 * @param max_msg_size Maximum size of each message in bytes
 * @param flags Queue behavior flags
 * @returns Pointer to queue, or NULL on failure
 */
async_queue_t* async_queue_create(size_t capacity, size_t max_msg_size, async_queue_flags_t flags);

/**
 * Destroy a queue and free all resources
 * 
 * @param queue Queue to destroy
 */
void async_queue_destroy(async_queue_t* queue);

/**
 * Enqueue a message (non-blocking unless BLOCK_WRITER flag set)
 * 
 * @param queue Queue to enqueue to
 * @param data Message data to copy
 * @param size Size of message data
 * @returns true on success, false if queue full (unless BLOCK_WRITER set)
 */
bool async_queue_enqueue(async_queue_t* queue, const void* data, size_t size);

/**
 * Dequeue a message (non-blocking)
 * 
 * @param queue Queue to dequeue from
 * @param buffer Buffer to copy message into
 * @param buffer_size Size of buffer
 * @param out_size Output: actual message size (can be NULL)
 * @returns true if message retrieved, false if queue empty
 */
bool async_queue_dequeue(async_queue_t* queue, void* buffer, size_t buffer_size, size_t* out_size);

/**
 * Check if queue is empty
 * 
 * @param queue Queue to check
 * @returns true if empty
 */
bool async_queue_is_empty(const async_queue_t* queue);

/**
 * Check if queue is full
 * 
 * @param queue Queue to check
 * @returns true if full
 */
bool async_queue_is_full(const async_queue_t* queue);

/**
 * Clear all messages from queue
 * 
 * @param queue Queue to clear
 */
void async_queue_clear(async_queue_t* queue);

/**
 * Get queue statistics
 * 
 * @param queue Queue to query
 * @param stats Output: statistics structure
 */
void async_queue_get_stats(const async_queue_t* queue, async_queue_stats_t* stats);

#endif /* ASYNC_QUEUE_H */
