# Async Library User Guide

**Library**: `lib/async`  
**Status**: Design Phase  
**Platform Support**: Windows, Linux, macOS

## Overview

The async library provides thread-safe message passing and worker thread management for Neolith. It enables clean separation between blocking I/O operations (which run in worker threads) and the main event loop (which must never block).

**When to use async library**:
- ✅ Blocking I/O operations (console input, file reads, network requests)
- ✅ CPU-intensive tasks that would freeze the game
- ✅ Cross-thread communication with main event loop
- ❌ Simple callbacks (use function pointers)
- ❌ Short-lived computations (overhead > benefit)

## Quick Start

### Basic Worker Thread

```c
#include "async/async_worker.h"
#include "async/async_queue.h"

// Worker thread procedure
void* background_task(void* context) {
    int task_id = *(int*)context;
    async_worker_t* self = async_worker_current();
    
    // Do work until stop signal received
    while (!async_worker_should_stop(self)) {
        // Blocking operation (OK in worker thread)
        perform_blocking_io();
    }
    
    return NULL;
}

// Start worker from main thread
int task_id = 42;
async_worker_t* worker = async_worker_create(background_task, &task_id, 0);
if (!worker) {
    // Handle creation failure
    return ERROR_THREAD_CREATE;
}

// Later: signal worker to stop
async_worker_signal_stop(worker);

// Wait up to 5 seconds for clean shutdown
if (!async_worker_join(worker, 5000)) {
    // Worker didn't stop in time
    log_error("Worker thread timeout");
}

async_worker_destroy(worker);
```

### Message Queue (Worker → Main Thread)

```c
#include "async/async_queue.h"

// Create queue in main thread
async_queue_t* queue = async_queue_create(
    256,                        // 256 messages max
    512,                        // 512 bytes per message
    ASYNC_QUEUE_DROP_OLDEST     // Drop old messages when full
);

// Worker thread: send messages
void* worker_proc(void* ctx) {
    async_queue_t* q = (async_queue_t*)ctx;
    
    while (!async_worker_should_stop(async_worker_current())) {
        char message[512];
        size_t len = read_data_from_source(message, sizeof(message));
        
        if (len > 0) {
            // Non-blocking enqueue
            if (!async_queue_enqueue(q, message, len)) {
                // Queue full (oldest dropped if DROP_OLDEST flag set)
            }
        }
    }
    
    return NULL;
}

// Main thread: receive messages
void process_queue(async_queue_t* queue) {
    char message[512];
    size_t message_len;
    
    // Drain all available messages
    while (async_queue_dequeue(queue, message, sizeof(message), &message_len)) {
        handle_message(message, message_len);
    }
}
```

## Components

### 1. Message Queue (async_queue)

Thread-safe FIFO queue for passing data between threads.

#### Creating a Queue

```c
/**
 * @param capacity Maximum number of messages (must be power of 2 for best performance)
 * @param max_message_size Maximum size of each message in bytes
 * @param flags Behavior flags (can be OR'd together)
 */
async_queue_t* async_queue_create(
    size_t capacity,
    size_t max_message_size,
    async_queue_flags_t flags
);
```

**Flags**:
- `ASYNC_QUEUE_DROP_OLDEST`: Drop oldest message when queue full (default: enqueue fails)
- `ASYNC_QUEUE_BLOCK_WRITER`: Block enqueue until space available (use with caution)
- `ASYNC_QUEUE_SIGNAL_ON_DATA`: Signal event when data enqueued (for polling)

**Example**:
```c
// Small queue for high-priority events (fail if full)
async_queue_t* events = async_queue_create(32, 256, 0);

// Large queue for logging (drop old logs if full)
async_queue_t* logs = async_queue_create(1024, 1024, ASYNC_QUEUE_DROP_OLDEST);

// Command queue with blocking writes
async_queue_t* commands = async_queue_create(128, 512, ASYNC_QUEUE_BLOCK_WRITER);
```

#### Sending Messages

```c
bool async_queue_enqueue(async_queue_t* queue, const void* data, size_t size);
```

**Returns**: `true` on success, `false` if queue full (unless `BLOCK_WRITER` set)

**Thread Safety**: Safe to call from multiple threads (multiple producers)

**Example**:
```c
typedef struct {
    int event_type;
    char payload[256];
} event_message_t;

event_message_t msg = {
    .event_type = EVENT_USER_LOGIN,
    .payload = "player123"
};

if (!async_queue_enqueue(event_queue, &msg, sizeof(msg))) {
    log_warning("Event queue full, message dropped");
}
```

#### Receiving Messages

```c
bool async_queue_dequeue(
    async_queue_t* queue,
    void* buffer,
    size_t buffer_size,
    size_t* out_size
);
```

**Returns**: `true` if message retrieved, `false` if queue empty

**Thread Safety**: Safe from single consumer (MPSC pattern)

**Example**:
```c
// Typical main loop usage
void main_loop_iteration(async_queue_t* queue) {
    event_message_t msg;
    size_t msg_size;
    
    while (async_queue_dequeue(queue, &msg, sizeof(msg), &msg_size)) {
        switch (msg.event_type) {
            case EVENT_USER_LOGIN:
                handle_login(msg.payload);
                break;
            // ... other event types
        }
    }
}
```

#### Queue Status

```c
bool async_queue_is_empty(const async_queue_t* queue);
bool async_queue_is_full(const async_queue_t* queue);

async_queue_stats_t stats;
async_queue_get_stats(queue, &stats);
printf("Queue depth: %zu/%zu (dropped: %zu)\n",
       stats.current_size, stats.capacity, stats.dropped_count);
```

#### Cleanup

```c
void async_queue_destroy(async_queue_t* queue);
```

**Behavior**: Frees all resources. If queue not empty, remaining messages are lost.

### 2. Worker Threads (async_worker)

Managed threads with graceful shutdown support.

#### Creating a Worker

```c
async_worker_t* async_worker_create(
    async_worker_proc_t proc,    // Thread procedure
    void* context,               // User context passed to proc
    size_t stack_size            // Stack size (0 = default)
);
```

**Thread Procedure Signature**:
```c
void* my_worker_proc(void* context);
```

**Example**:
```c
typedef struct {
    async_queue_t* input_queue;
    async_queue_t* output_queue;
    const char* config_file;
} worker_context_t;

void* worker_proc(void* ctx) {
    worker_context_t* wctx = (worker_context_t*)ctx;
    async_worker_t* self = async_worker_current();
    
    // Initialize worker-local resources
    load_config(wctx->config_file);
    
    // Main work loop
    while (!async_worker_should_stop(self)) {
        // Get work from input queue
        task_t task;
        size_t task_size;
        if (async_queue_dequeue(wctx->input_queue, &task, sizeof(task), &task_size)) {
            // Process task (blocking OK)
            result_t result = process_task(&task);
            
            // Post result to output queue
            async_queue_enqueue(wctx->output_queue, &result, sizeof(result));
        } else {
            // No work available - sleep briefly
            usleep(1000);  // 1ms
        }
    }
    
    // Cleanup worker-local resources
    unload_config();
    return NULL;
}

// Start worker
worker_context_t ctx = { in_queue, out_queue, "worker.conf" };
async_worker_t* worker = async_worker_create(worker_proc, &ctx, 0);
```

#### Stopping a Worker

```c
// Signal worker to stop (non-blocking)
async_worker_signal_stop(worker);

// Wait for worker to finish (blocks up to timeout)
if (!async_worker_join(worker, 5000)) {
    log_error("Worker did not stop within 5 seconds");
    // Worker is still running - forced shutdown or leak?
}

// Always call destroy to free resources
async_worker_destroy(worker);
```

**Best Practice**: Worker should check `async_worker_should_stop()` frequently (every iteration or every ~100ms).

#### Worker State

```c
async_worker_state_t state = async_worker_get_state(worker);
switch (state) {
    case ASYNC_WORKER_STOPPED:  // Not running
        break;
    case ASYNC_WORKER_RUNNING:  // Currently executing
        break;
    case ASYNC_WORKER_STOPPING: // Stop signal sent, not yet stopped
        break;
}
```

### 3. Async Runtime (Event Loop Integration)

**Note**: Synchronization primitives (mutexes, events) are internal to the async library implementation (located in `lib/port/port_sync.{h,c}`). User code should not call these directly - use the higher-level async_queue and async_worker APIs instead.

The async runtime (`lib/async/async_runtime`) provides a unified event loop that handles both I/O events (sockets) and worker completions. Worker threads post completions directly to the runtime, which are then processed in the main event loop alongside network I/O.

#### Unified Event Loop

```c
#include "async/async_runtime.h"

// Runtime is created once during backend initialization
async_runtime_t* runtime = async_runtime_init();

// Worker thread: post completion
#define MY_COMPLETION_KEY 0x12345
async_runtime_post_completion(runtime, MY_COMPLETION_KEY, bytes_processed);

// Main thread: unified event loop receives I/O and worker events
io_event_t events[64];
int n = async_runtime_wait(runtime, events, 64, &timeout);

for (int i = 0; i < n; i++) {
    if (events[i].completion_key == MY_COMPLETION_KEY) {
        // Worker completed - check queue for results
        process_worker_results();
    } else {
        // I/O event (socket ready)
        handle_socket_io(&events[i]);
    }
}
```

**Platform Implementation**:
- **Windows**: Uses IOCP for unified I/O and completion handling
- **Linux**: Uses epoll for I/O + internal eventfd for worker completions
- **macOS/BSD**: Uses poll for I/O + internal pipe for worker completions

All platforms provide the same API through `async_runtime_wait()`, which returns events from both I/O sources and worker threads in a single call.

## Common Patterns

### Pattern 1: Background File Processing

**Problem**: Loading large files blocks the main event loop.

**Solution**: Worker thread loads files, posts results to queue.

```c
typedef struct {
    char filename[256];
    void* data;
    size_t size;
    int status;
} file_result_t;

async_queue_t* result_queue;
async_worker_t* file_worker;

void* file_worker_proc(void* ctx) {
    async_queue_t* work_queue = (async_queue_t*)ctx;
    
    while (!async_worker_should_stop(async_worker_current())) {
        char filename[256];
        size_t name_len;
        
        if (async_queue_dequeue(work_queue, filename, sizeof(filename), &name_len)) {
            file_result_t result = {0};
            strcpy(result.filename, filename);
            
            // Blocking file read
            FILE* f = fopen(filename, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                result.size = ftell(f);
                fseek(f, 0, SEEK_SET);
                
                result.data = malloc(result.size);
                if (result.data) {
                    fread(result.data, 1, result.size, f);
                    result.status = 0;  // Success
                } else {
                    result.status = -1;  // Out of memory
                }
                fclose(f);
            } else {
                result.status = -2;  // File not found
            }
            
            // Post result
            async_queue_enqueue(result_queue, &result, sizeof(result));
        }
    }
    
    return NULL;
}

// Main thread: request file load
void load_file_async(const char* filename) {
    async_queue_enqueue(work_queue, filename, strlen(filename) + 1);
}

// Main thread: process results
void process_file_results(void) {
    file_result_t result;
    size_t result_size;
    
    while (async_queue_dequeue(result_queue, &result, sizeof(result), &result_size)) {
        if (result.status == 0) {
            // File loaded successfully
            handle_file_data(result.filename, result.data, result.size);
            free(result.data);
        } else {
            log_error("Failed to load %s: %d", result.filename, result.status);
        }
    }
}
```

### Pattern 2: Console Input with IOCP Notification (Windows)

**Problem**: Console input blocks, and polling adds latency.

**Solution**: Worker reads console, posts IOCP completion to wake reactor.

```c
// See docs/plan/console-async.md for full details

async_queue_t* line_queue;
async_runtime_t* runtime;  // Shared with backend
async_worker_t* console_worker;

#define CONSOLE_COMPLETION_KEY 0xC0701E

typedef struct {
    async_queue_t* queue;
    async_runtime_t* runtime;
} console_context_t;

void* console_worker_proc(void* ctx) {
    console_context_t* cctx = (console_context_t*)ctx;
    HANDLE stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
    
    while (!async_worker_should_stop(async_worker_current())) {
        char line[4096];
        DWORD bytes_read;
        
        // Blocking read with native line editing
        if (ReadConsole(stdin_handle, line, sizeof(line) - 1, &bytes_read, NULL)) {
            if (bytes_read > 0) {
                line[bytes_read] = '\0';
                async_queue_enqueue(cctx->queue, line, bytes_read);
                
                // Wake runtime INSTANTLY (no 60s timeout!)
                async_runtime_post_completion(cctx->runtime, CONSOLE_COMPLETION_KEY, bytes_read);
            }
        }
    }
    
    return NULL;
}

// Main event loop
while (running) {
    io_event_t events[64];
    int n = async_runtime_wait(runtime, events, 64, &timeout);
    
    for (int i = 0; i < n; i++) {
        if (events[i].completion_key == CONSOLE_COMPLETION_KEY) {
            // Process console input
            char line[4096];
            size_t len;
            while (async_queue_dequeue(line_queue, line, sizeof(line), &len)) {
                process_user_command(line);
            }
        }
    }
}
```

### Pattern 3: Bidirectional Channel (GUI Client, MCP Server)

**Problem**: Need two-way communication between main thread and external client.

**Solution**: Two queues (input/output) with reader/writer workers.

```c
// Bidirectional channel abstraction
typedef struct {
    async_queue_t* input_queue;    // Client → Driver
    async_queue_t* output_queue;   // Driver → Client
    async_worker_t* reader;        // Worker reads from client socket/pipe
    async_worker_t* writer;        // Worker writes to client socket/pipe
    async_runtime_t* runtime;      // Event loop runtime (shared)
    uintptr_t channel_key;         // Completion key for this channel
    int socket_fd;                 // Or HANDLE on Windows
    volatile bool connected;
} bidirectional_channel_t;

// Reader worker: Client → Driver
void* channel_reader_proc(void* ctx) {
    bidirectional_channel_t* channel = (bidirectional_channel_t*)ctx;
    
    while (!async_worker_should_stop(async_worker_current())) {
        char buffer[4096];
        ssize_t bytes_read = read(channel->socket_fd, buffer, sizeof(buffer));
        
        if (bytes_read <= 0) {
            channel->connected = false;
            break;  // Disconnected
        }
        
        // Enqueue input
        async_queue_enqueue(channel->input_queue, buffer, bytes_read);
        
        // Notify main thread
        async_runtime_post_completion(channel->runtime, channel->channel_key, bytes_read);
    }
    
    return NULL;
}

// Writer worker: Driver → Client
void* channel_writer_proc(void* ctx) {
    bidirectional_channel_t* channel = (bidirectional_channel_t*)ctx;
    
    while (!async_worker_should_stop(async_worker_current())) {
        char buffer[4096];
        size_t msg_len;
        
        if (async_queue_dequeue(channel->output_queue, buffer, sizeof(buffer), &msg_len)) {
            ssize_t written = write(channel->socket_fd, buffer, msg_len);
            if (written != (ssize_t)msg_len) {
                channel->connected = false;
                break;  // Write error
            }
        } else {
            usleep(1000);  // No data, sleep briefly
        }
    }
    
    return NULL;
}

// Main thread: Create channel
bidirectional_channel_t* create_channel(int socket_fd, async_runtime_t* runtime, uintptr_t channel_key) {
    bidirectional_channel_t* ch = calloc(1, sizeof(*ch));
    
    ch->socket_fd = socket_fd;
    ch->runtime = runtime;
    ch->channel_key = channel_key;
    ch->connected = true;
    
    ch->input_queue = async_queue_create(256, 4096, ASYNC_QUEUE_DROP_OLDEST);
    ch->output_queue = async_queue_create(256, 4096, ASYNC_QUEUE_DROP_OLDEST);
    
    ch->reader = async_worker_create(channel_reader_proc, ch, 0);
    ch->writer = async_worker_create(channel_writer_proc, ch, 0);
    
    return ch;
}

// Main thread: Send to client
void channel_send(bidirectional_channel_t* ch, const char* data, size_t len) {
    async_queue_enqueue(ch->output_queue, data, len);
}

// Main thread: Receive from client
bool channel_receive(bidirectional_channel_t* ch, char* buffer, size_t max_len, size_t* out_len) {
    return async_queue_dequeue(ch->input_queue, buffer, max_len, out_len);
}

// Cleanup
void destroy_channel(bidirectional_channel_t* ch) {
    async_worker_signal_stop(ch->reader);
    async_worker_signal_stop(ch->writer);
    async_worker_join(ch->reader, 5000);
    async_worker_join(ch->writer, 5000);
    async_worker_destroy(ch->reader);
    async_worker_destroy(ch->writer);
    async_queue_destroy(ch->input_queue);
    async_queue_destroy(ch->output_queue);
    close(ch->socket_fd);
    free(ch);
}
```

**Use Cases**: GUI client connections, MCP server stdio, SSH-like remote access.

### Pattern 4: Request/Response Correlation (REST API, JSON-RPC)

**Problem**: Async operations need to match responses to original requests.

**Solution**: Add correlation ID to messages, track pending requests.

```c
// Request with correlation
typedef struct {
    uint64_t request_id;       // Generated by sender
    char url[512];
    char method[16];           // GET, POST, PUT, DELETE
    char* request_body;        // Allocated (NULL if GET)
    size_t request_body_len;
} http_request_t;

// Response with correlation
typedef struct {
    uint64_t request_id;       // Matches original request
    int status_code;           // 200, 404, 500, etc.
    char* response_body;       // Allocated in worker, freed in main
    size_t response_body_len;
    int error_code;            // 0 = success, non-zero = network error
    char error_message[256];
} http_response_t;

// Request tracking
typedef struct {
    uint64_t request_id;
    void (*callback)(http_response_t* response, void* context);
    void* callback_context;
    time_t timestamp;          // For timeout detection
} pending_request_t;

static uint64_t next_request_id = 1;
static pending_request_t pending_requests[MAX_PENDING];

// Main thread: Send request
uint64_t send_http_request(const char* url, const char* method, 
                            void (*callback)(http_response_t*, void*), void* ctx) {
    http_request_t req = {0};
    req.request_id = next_request_id++;
    strncpy(req.url, url, sizeof(req.url) - 1);
    strncpy(req.method, method, sizeof(req.method) - 1);
    
    // Track pending request
    pending_request_t* pending = &pending_requests[req.request_id % MAX_PENDING];
    pending->request_id = req.request_id;
    pending->callback = callback;
    pending->callback_context = ctx;
    pending->timestamp = time(NULL);
    
    // Send to worker
    async_queue_enqueue(http_request_queue, &req, sizeof(req));
    
    return req.request_id;
}

// Worker thread: Process request
void* http_worker_proc(void* ctx) {
    while (!async_worker_should_stop(async_worker_current())) {
        http_request_t req;
        size_t req_size;
        
        if (async_queue_dequeue(http_request_queue, &req, sizeof(req), &req_size)) {
            http_response_t resp = {0};
            resp.request_id = req.request_id;  // CORRELATION KEY
            
            // Perform HTTP request (blocking)
            resp.status_code = perform_http_request(req.url, req.method, 
                                                     req.request_body,
                                                     &resp.response_body,
                                                     &resp.response_body_len,
                                                     &resp.error_code);
            
            if (resp.error_code != 0) {
                snprintf(resp.error_message, sizeof(resp.error_message),
                         "HTTP request failed: %d", resp.error_code);
            }
            
            // Send response back
            async_queue_enqueue(http_response_queue, &resp, sizeof(resp));
            async_runtime_post_completion(runtime, HTTP_RESPONSE_KEY, resp.request_id);
            
            free(req.request_body);  // Cleanup request
        }
    }
    
    return NULL;
}

// Main thread: Process responses
void process_http_responses(void) {
    http_response_t resp;
    size_t resp_size;
    
    while (async_queue_dequeue(http_response_queue, &resp, sizeof(resp), &resp_size)) {
        // Find pending request by ID
        pending_request_t* pending = &pending_requests[resp.request_id % MAX_PENDING];
        
        if (pending->request_id == resp.request_id) {
            // Invoke callback
            if (pending->callback) {
                pending->callback(&resp, pending->callback_context);
            }
            
            // Clear pending
            pending->request_id = 0;
            pending->callback = NULL;
        } else {
            log_warning("Received response for unknown request: %lu", resp.request_id);
        }
        
        free(resp.response_body);  // Cleanup response
    }
}
```

**Use Cases**: REST API calls, JSON-RPC servers (MCP), distributed computing.

### Pattern 5: Progress Reporting (Git, Large File Ops)

**Problem**: Long-running operations need to report progress without blocking.

**Solution**: Worker periodically posts progress messages to dedicated queue.

```c
// Progress message
typedef struct {
    uint64_t task_id;
    int progress_percent;      // 0-100
    char status_message[256];
    bool completed;
    int exit_code;             // Final result (0 = success)
} task_progress_t;

// Worker: Git clone with progress
void* git_clone_worker(void* ctx) {
    git_clone_params_t* params = (git_clone_params_t*)ctx;
    task_progress_t progress = {0};
    progress.task_id = params->task_id;
    
    // Start
    progress.progress_percent = 0;
    strcpy(progress.status_message, "Initializing git clone...");
    async_queue_enqueue(progress_queue, &progress, sizeof(progress));
    async_runtime_post_completion(runtime, PROGRESS_UPDATE_KEY, progress.task_id);
    
    // Clone with progress callbacks
    for (int phase = 0; phase < 3; phase++) {
        switch (phase) {
            case 0:
                progress.progress_percent = 10;
                strcpy(progress.status_message, "Resolving deltas...");
                break;
            case 1:
                progress.progress_percent = 50;
                strcpy(progress.status_message, "Receiving objects...");
                break;
            case 2:
                progress.progress_percent = 90;
                strcpy(progress.status_message, "Checking out files...");
                break;
        }
        
        async_queue_enqueue(progress_queue, &progress, sizeof(progress));
        async_runtime_post_completion(runtime, PROGRESS_UPDATE_KEY, progress.task_id);
        
        // Do actual work
        perform_git_phase(phase, params);
    }
    
    // Completion
    progress.completed = true;
    progress.progress_percent = 100;
    progress.exit_code = 0;  // Success
    strcpy(progress.status_message, "Clone completed successfully");
    async_queue_enqueue(progress_queue, &progress, sizeof(progress));
    async_runtime_post_completion(runtime, PROGRESS_COMPLETE_KEY, progress.task_id);
    
    return NULL;
}

// Main thread: Display progress
void update_task_progress(void) {
    task_progress_t progress;
    size_t progress_size;
    
    while (async_queue_dequeue(progress_queue, &progress, sizeof(progress), &progress_size)) {
        // Update UI or log
        log_info("Task %lu: [%d%%] %s", 
                 progress.task_id, progress.progress_percent, progress.status_message);
        
        if (progress.completed) {
            if (progress.exit_code == 0) {
                notify_user("Task completed successfully");
            } else {
                notify_user("Task failed with code %d", progress.exit_code);
            }
        }
    }
}
```

**Use Cases**: Git operations, file compression, database imports, LPC recompilation.

### Pattern 6: MCP Server (JSON-RPC over stdio)

**Problem**: Implement Model Context Protocol server for AI tool integration.

**Solution**: Bidirectional JSON-RPC with request/response correlation.

```c
// MCP message types
typedef enum {
    MCP_MSG_REQUEST,       // Client → Server (expects response)
    MCP_MSG_RESPONSE,      // Server → Client (answers request)
    MCP_MSG_NOTIFICATION   // Server → Client (no response expected)
} mcp_message_type_t;

typedef struct {
    mcp_message_type_t type;
    uint64_t id;           // Request ID (0 for notifications)
    char method[64];       // "tools/call", "resources/read", etc.
    char* params;          // JSON string (allocated)
    size_t params_len;
    char* result;          // For responses (allocated)
    size_t result_len;
} mcp_message_t;

// Global queues
async_queue_t* mcp_requests;   // Client → Server
async_queue_t* mcp_responses;  // Server → Client
async_runtime_t* mcp_runtime;  // Event loop runtime

// Reader worker: stdin → requests queue
void* mcp_reader_proc(void* ctx) {
    while (!async_worker_should_stop(async_worker_current())) {
        mcp_message_t msg = {0};
        
        // Read JSON-RPC from stdin (blocking)
        if (read_jsonrpc_line(STDIN_FILENO, &msg)) {
            async_queue_enqueue(mcp_requests, &msg, sizeof(msg));
            async_runtime_post_completion(mcp_runtime, MCP_REQUEST_KEY, msg.id);
        } else {
            break;  // EOF or error
        }
    }
    return NULL;
}

// Writer worker: responses queue → stdout
void* mcp_writer_proc(void* ctx) {
    while (!async_worker_should_stop(async_worker_current())) {
        mcp_message_t msg;
        size_t msg_size;
        
        if (async_queue_dequeue(mcp_responses, &msg, sizeof(msg), &msg_size)) {
            write_jsonrpc_line(STDOUT_FILENO, &msg);
            
            // Cleanup allocated strings
            free(msg.params);
            free(msg.result);
        } else {
            usleep(1000);  // No data
        }
    }
    return NULL;
}

// Main thread: Process MCP requests
void process_mcp_requests(void) {
    mcp_message_t req;
    size_t req_size;
    
    while (async_queue_dequeue(mcp_requests, &req, sizeof(req), &req_size)) {
        mcp_message_t resp = {0};
        resp.type = MCP_MSG_RESPONSE;
        resp.id = req.id;  // CORRELATION
        
        // Dispatch to tool handlers
        if (strcmp(req.method, "tools/call") == 0) {
            // Parse params, invoke LPC function, generate result
            resp.result = invoke_lpc_tool(req.params, &resp.result_len);
        } else if (strcmp(req.method, "resources/read") == 0) {
            // Read mudlib file
            resp.result = read_mudlib_resource(req.params, &resp.result_len);
        } else {
            // Unknown method
            resp.result = strdup("{\"error\":\"Method not found\"}");
            resp.result_len = strlen(resp.result);
        }
        
        // Send response
        async_queue_enqueue(mcp_responses, &resp, sizeof(resp));
        
        free(req.params);  // Cleanup request
    }
}

// Send notification (server → client, no response expected)
void send_mcp_notification(const char* method, const char* params) {
    mcp_message_t notif = {0};
    notif.type = MCP_MSG_NOTIFICATION;
    notif.id = 0;  // No correlation
    strncpy(notif.method, method, sizeof(notif.method) - 1);
    notif.params = strdup(params);
    notif.params_len = strlen(params);
    
    async_queue_enqueue(mcp_responses, &notif, sizeof(notif));
}

// Example: Notify AI when player connects
void on_player_login(object_t* player) {
    char params[512];
    snprintf(params, sizeof(params), 
             "{\"event\":\"player_login\",\"name\":\"%s\"}", 
             player->name);
    send_mcp_notification("notifications/player_event", params);
}
```

**Use Cases**: Claude Desktop integration, Cline/Cursor AI tools, remote debugging.

### Pattern 7: Multiple Workers Sharing Queue (Thread Pool)

**Problem**: Distribute tasks across multiple CPU cores.

**Solution**: Multiple workers dequeue from shared task queue.

```c
#define NUM_WORKERS 4

async_queue_t* task_queue = async_queue_create(1024, 512, 0);
async_worker_t* workers[NUM_WORKERS];

void* worker_proc(void* ctx) {
    int worker_id = *(int*)ctx;
    
    while (!async_worker_should_stop(async_worker_current())) {
        task_t task;
        size_t task_size;
        
        if (async_queue_dequeue(task_queue, &task, sizeof(task), &task_size)) {
            // Process task
            result_t result = perform_computation(&task);
            
            // Post result to per-worker queue or shared result queue
            post_result(worker_id, &result);
        }
    }
    
    return NULL;
}

// Start workers
int worker_ids[NUM_WORKERS];
for (int i = 0; i < NUM_WORKERS; i++) {
    worker_ids[i] = i;
    workers[i] = async_worker_create(worker_proc, &worker_ids[i], 0);
}

// Distribute tasks
for (int i = 0; i < 10000; i++) {
    task_t task = { .task_id = i };
    async_queue_enqueue(task_queue, &task, sizeof(task));
}

// Wait for all workers to finish
for (int i = 0; i < NUM_WORKERS; i++) {
    async_worker_signal_stop(workers[i]);
    async_worker_join(workers[i], 5000);
    async_worker_destroy(workers[i]);
}
```

## Best Practices

### 1. Queue Sizing

**Rule of Thumb**: Queue capacity should handle ~1 second of peak message rate.

```c
// Bad: Too small, drops messages under load
async_queue_t* q = async_queue_create(10, 1024, ASYNC_QUEUE_DROP_OLDEST);

// Good: Sized for expected load (100 messages/sec * 1 sec = 100)
async_queue_t* q = async_queue_create(128, 1024, ASYNC_QUEUE_DROP_OLDEST);

// Good: Large buffer for bursty traffic
async_queue_t* q = async_queue_create(1024, 512, ASYNC_QUEUE_DROP_OLDEST);
```

### 2. Worker Shutdown

**Always** provide timeout to `async_worker_join()`:

```c
// Bad: Infinite wait can deadlock
async_worker_signal_stop(worker);
async_worker_join(worker, -1);

// Good: Timeout with error handling
async_worker_signal_stop(worker);
if (!async_worker_join(worker, 5000)) {
    log_error("Worker shutdown timeout - possible deadlock");
    // Decision point: leak worker or force termination?
}
```

**Worker should poll shutdown flag frequently**:

```c
// Bad: Long blocking calls without checking
void* worker_proc(void* ctx) {
    while (1) {  // Never checks should_stop!
        blocking_operation_that_takes_minutes();
    }
}

// Good: Check every iteration
void* worker_proc(void* ctx) {
    while (!async_worker_should_stop(async_worker_current())) {
        if (blocking_operation_with_timeout(1000)) {
            // Process result
        }
        // Checks should_stop every 1 second
    }
}
```

### 3. Thread Safety

**Queues are thread-safe, but data passed through them is not**:

```c
// Bad: Passing pointer to stack data
void bad_example(void) {
    char local_buffer[256] = "Hello";
    async_queue_enqueue(queue, &local_buffer, sizeof(local_buffer));
    // local_buffer destroyed when function returns!
}

// Good: Copy data into queue
void good_example(void) {
    char message[256] = "Hello";
    async_queue_enqueue(queue, message, sizeof(message));  // Copies data
}

// Also good: Heap allocation with ownership transfer
void also_good_example(void) {
    char* message = malloc(256);
    strcpy(message, "Hello");
    async_queue_enqueue(queue, &message, sizeof(message));  // Pass pointer
    // Consumer must free(message) after dequeue
}
```

### 4. Error Handling

**Check all creation failures**:

```c
async_queue_t* queue = async_queue_create(256, 1024, 0);
if (!queue) {
    log_error("Failed to create queue: out of memory");
    return ERROR_INIT_FAILED;
}

async_worker_t* worker = async_worker_create(worker_proc, queue, 0);
if (!worker) {
    log_error("Failed to create worker thread");
    async_queue_destroy(queue);
    return ERROR_THREAD_CREATE;
}
```

**Graceful degradation on queue full**:

```c
// Drop silently (for non-critical messages like debug logs)
async_queue_enqueue(log_queue, message, message_len);

// Warn and continue (for important but recoverable events)
if (!async_queue_enqueue(event_queue, &event, sizeof(event))) {
    log_warning("Event queue full, event dropped: %d", event.type);
}

// Error and fail (for critical data)
if (!async_queue_enqueue(save_queue, &save_data, sizeof(save_data))) {
    log_error("Save queue full - cannot guarantee data persistence!");
    return ERROR_QUEUE_FULL;
}
```

## Performance Tips

### 1. Batch Processing

**Dequeue multiple messages per iteration**:

```c
// Less efficient: Process one message per main loop iteration
if (async_queue_dequeue(queue, &msg, sizeof(msg), &len)) {
    process_message(&msg);
}

// More efficient: Drain queue each iteration
while (async_queue_dequeue(queue, &msg, sizeof(msg), &len)) {
    process_message(&msg);
}
```

### 2. Message Size

**Use fixed-size messages** for predictable memory usage:

```c
// Good: Fixed size, pre-allocated
typedef struct {
    int type;
    char payload[256];
} fixed_message_t;

async_queue_t* q = async_queue_create(256, sizeof(fixed_message_t), 0);
```

**For variable-size data, pass pointers**:

```c
// Good: Small message with heap-allocated payload
typedef struct {
    int type;
    char* data;      // Allocated in worker, freed in main thread
    size_t data_len;
} var_message_t;

async_queue_t* q = async_queue_create(256, sizeof(var_message_t), 0);
```

### 3. Avoid Locks in Hot Paths

**If queue is only accessed by one reader and one writer**, consider requesting lock-free implementation (future enhancement).

## Debugging

### Queue Statistics

```c
async_queue_stats_t stats;
async_queue_get_stats(queue, &stats);

log_debug("Queue stats: %zu/%zu messages (%.1f%% full)",
          stats.current_size, stats.capacity,
          100.0 * stats.current_size / stats.capacity);

if (stats.dropped_count > 0) {
    log_warning("Queue dropped %zu messages (queue too small or consumer too slow)",
                stats.dropped_count);
}
```

### Deadlock Detection

**Common causes**:
1. Worker waiting for queue space (if `BLOCK_WRITER` set)
2. Main thread waiting for worker join, but worker blocked on queue

**Solution**: Always use timeouts and check for circular dependencies.

### Memory Leaks

**Check with Valgrind** (Linux):
```bash
valgrind --leak-check=full ./neolith -f config.conf
```

**Check with Dr. Memory** (Windows):
```powershell
drmemory -- neolith.exe -f config.conf
```

## Migration from Existing Code

If you have existing thread code, migrate gradually:

1. **Replace manual thread creation** with `async_worker_create()`
2. **Replace custom queues** with `async_queue_create()`
3. **Replace CRITICAL_SECTION/pthread_mutex** with `async_mutex_t` (optional)
4. **Add notifier integration** for event loop wakeup (if needed)

**Example migration**:

```c
// Before: Manual Windows thread
HANDLE thread = CreateThread(NULL, 0, worker_proc, ctx, 0, NULL);
WaitForSingleObject(thread, INFINITE);
CloseHandle(thread);

// After: async_worker
async_worker_t* worker = async_worker_create(worker_proc, ctx, 0);
async_worker_signal_stop(worker);
async_worker_join(worker, 5000);
async_worker_destroy(worker);
```

## See Also

### Design Documentation
- [Async Library Design](../internals/async-library.md) - Internal architecture details
- [Async Library Use Case Analysis](../internals/async-library-use-case-analysis.md) - Extended use cases evaluation

### Implementation Examples
- [Console Async Integration](../plan/console-async.md) - Console worker implementation
- [Timer Port API](../internals/timer-port.md) - Existing thread abstraction for timers

### Integration
- [Windows I/O Reactor](windows-io.md) - IOCP integration details
- [Linux I/O Reactor](linux-io.md) - poll/epoll integration

### Future Use Cases
See [async-library-use-case-analysis.md](../internals/async-library-use-case-analysis.md) for detailed analysis of:
- GUI client connections (bidirectional channels)
- REST API integration (HTTP client with async workers)
- Git operations (mudlib auto-update with progress reporting)
- MCP server (AI tool integration via JSON-RPC)

---

**Questions?** Open an issue or discuss in [CONTRIBUTING.md](../CONTRIBUTING.md).
