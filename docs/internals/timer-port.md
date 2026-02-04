# Timer Port API

## Overview

The timer port API provides a platform-agnostic interface for periodic timer callbacks in the Neolith driver. It uses a single portable C++11 implementation leveraging:

- **`std::thread`**: Background timer execution
- **`std::chrono::steady_clock`**: High-precision monotonic timing
- **`std::condition_variable`**: Efficient waiting without busy-polling
- **`std::mutex`** and **`std::atomic`**: Thread-safe state management

The C API is defined in [lib/port/timer_port.h](../../lib/port/timer_port.h), implemented in [lib/port/timer_port.cpp](../../lib/port/timer_port.cpp).

## Error Handling

The API uses platform-agnostic error codes returned via `timer_error_t` enum:

| Error Code | Value | Description |
|------------|-------|-------------|
| `TIMER_OK` | 0 | Success |
| `TIMER_ERR_NULL_PARAM` | -1 | Null pointer parameter |
| `TIMER_ERR_ALREADY_ACTIVE` | -2 | Timer already running |
| `TIMER_ERR_NOT_ACTIVE` | -3 | Timer not running (unused) |
| `TIMER_ERR_SYSTEM` | -4 | System call failed |
| `TIMER_ERR_THREAD` | -5 | Thread creation failed |
| `TIMER_ERR_INVALID_INTERVAL` | -6 | Zero interval not allowed |

### Error String Function

```c
const char* timer_error_string(timer_error_t err);
```

Converts error codes to human-readable strings for logging/debugging.

## API Functions

### timer_port_init()

```c
timer_error_t timer_port_init(timer_port_t *timer);
```

Initializes timer structure. Must be called before `timer_port_start()`.

**Parameters**:
- `timer`: Pointer to timer structure

**Returns**:
- `TIMER_OK`: Initialization successful
- `TIMER_ERR_NULL_PARAM`: timer is NULL
- `TIMER_ERR_SYSTEM`: System resource allocation failed (Windows/fallback only)

**Implementation Behavior**:
- Allocates internal `timer_port_internal` structure containing C++ objects
- Initializes `std::atomic` flags to inactive state
- Zero-initializes interval and callback pointer

### timer_port_start()

```c
timer_error_t timer_port_start(timer_port_t *timer, 
                               uint64_t interval_us,
                               timer_callback_t callback);
```

Starts periodic timer with specified interval and callback.

**Parameters**:
- `timer`: Initialized timer structure
- `interval_us`: Interval in microseconds (must be > 0)
- `callback`: Function to call on each tick

**Returns**:
- `TIMER_OK`: Timer started successfully
- `TIMER_ERR_NULL_PARAM`: timer or callback is NULL
- `TIMER_ERR_INVALID_INTERVAL`: interval_us is 0
- `TIMER_ERR_ALREADY_ACTIVE`: Timer already running
- `TIMER_ERR_SYSTEM`: System call failed (POSIX: signal/timer setup)
- `TIMER_ERR_THREAD`: Thread creation failed (Windows/fallback)

**Implementation Behavior**:
- Stores interval as `std::chrono::microseconds`
- Stores callback function pointer
- Sets `active` and `stop_requested` atomic flags
- Creates `std::thread` executing timer loop
- Timer loop uses `std::condition_variable::wait_until()` for precise timing
- Implements drift correction by maintaining absolute next-tick time

**Important**: Timer must be started only once during initialization. Calling this while timer is active returns `TIMER_ERR_ALREADY_ACTIVE`.

### timer_port_stop()

```c
timer_error_t timer_port_stop(timer_port_t *timer);
```

Stops active timer.

**Parameters**:
- `timer`: Timer to stop

**Returns**:
- `TIMER_OK`: Timer stopped successfully
- `TIMER_ERR_NULL_PARAM`: timer is NULL

**Implementation Behavior**:
- Sets `stop_requested` atomic flag to signal thread exit
- Notifies `condition_variable` to wake sleeping thread
- Calls `std::thread::join()` to wait for thread completion
- Clears callback pointer

**Note**: Safe to call even if timer not running (idempotent).

## Data Structures

### timer_port_t

Opaque handle to internal C++ timer state:

```c
typedef struct {
    void* internal;  /* Points to timer_port_internal structure */
} timer_port_t;
```

The `internal` pointer references a C++ structure (not exposed in the public API):

```cpp
struct timer_port_internal {
    std::thread timer_thread;
    std::mutex mutex;
    std::condition_variable cv;
    std::chrono::microseconds interval;
    timer_callback_t callback;
    std::atomic<bool> active;
    std::atomic<bool> stop_requested;
};
```

### timer_callback_t

```c
typedef void (*timer_callback_t)(void);
```

Callback function signature. 

**Execution Context**: Callback runs in the timer thread created by `std::thread`, separate from the main driver thread.

**Thread Safety**: Since the callback executes in a separate thread, it must be careful with shared state. In Neolith, the callback sets `heart_beat_flag = 1` (atomic operation) and on Windows calls `async_runtime_wakeup()` to interrupt the event loop.

## Integration with Backend

The driver uses a single global timer for heartbeat processing:

```c
static timer_port_t heartbeat_timer;
```

### Initialization Pattern

In [backend()](../../src/backend.c#L212-L226):

```c
timer_error_t timer_err;

timer_err = timer_port_init(&heartbeat_timer);
if (timer_err != TIMER_OK) {
    opt_warn(0, "Timer initialization failed: %s. heart_beat(), call_out() and reset() disabled.",
             timer_error_string(timer_err));
} else {
    timer_err = timer_port_start(&heartbeat_timer, HEARTBEAT_INTERVAL, heartbeat_timer_callback);
    if (timer_err != TIMER_OK) {
        opt_warn(0, "Timer start failed: %s. heart_beat(), call_out() and reset() disabled.",
                 timer_error_string(timer_err));
    }
}
```

**Critical**: Timer is started once during initialization, NOT restarted in callback.

### Callback Function

```c
static void heartbeat_timer_callback(void) {
    heart_beat_flag = 1;
    
#ifdef _WIN32
    /* Wake up async runtime to interrupt GetQueuedCompletionStatusEx() */
    async_runtime_t *reactor = get_async_runtime();
    if (reactor) {
        async_runtime_wakeup(reactor);
    }
#endif
}
```

The callback sets a flag checked by the main event loop. On Windows, it also wakes the IOCP event loop to ensure timely processing (on POSIX, signals naturally interrupt blocking calls). The actual heartbeat processing happens in `call_heart_beat()`, which does NOT restart the timer.

### Main Loop Integration

```c
while (1) {
    if (heart_beat_flag) {
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
    } else {
        timeout.tv_sec = 60;
        timeout.tv_usec = 0;
    }
    
    nb = do_comm_polling(&timeout);
    
    if (heart_beat_flag) {
        call_heart_beat();
    }
}
```

When `heart_beat_flag` is set, the main loop uses zero timeout for immediate processing, then calls `call_heart_beat()` to process all heart_beat objects.

## Configuration

- **`HEARTBEAT_INTERVAL`**: Defined in [src/backend.h](../../src/backend.h), default 2 seconds (2,000,000 microseconds)
- Timer interval configured at CMake time based on platform capabilities
- All three implementations support microsecond precision

## Build Requirements

- **C++11 compiler**: Required for `std::thread`, `std::chrono`, `std::condition_variable`
- **Thread library**: CMake automatically links `Threads::Threads` (pthread on Unix, native on Windows)
- **No platform-specific dependencies**: Replaces previous librt (POSIX) and kernel32 (Windows) requirements

See [lib/port/CMakeLists.txt](../../lib/port/CMakeLists.txt) for build configuration.

## Testing

Timer integration tests in [tests/test_backend/test_backend_timer.cpp](../../tests/test_backend/test_backend_timer.cpp):

- `HeartBeatFlagSetByTimer`: Verifies callback sets flag
- `MultipleTimerCallbacks`: Verifies periodic behavior
- `TimerStopPreventsFurtherCallbacks`: Verifies stop works
- `TimerRestart`: Verifies start/stop/start cycle
- `TimerActiveStatus`: Verifies active flag tracking
- `HeartBeatIntervalTiming`: Verifies timing accuracy
- `QueryHeartBeatIntegration`: Integration with query_heart_beat()

All tests verify error codes using `TIMER_OK` constant.

## Common Pitfalls

1. **Don't restart timer in callback**: Timer runs continuously; restarting returns `TIMER_ERR_ALREADY_ACTIVE`
2. **Thread safety**: Callback executes in separate thread - use atomic operations or proper synchronization for shared state
3. **Zero interval**: Returns `TIMER_ERR_INVALID_INTERVAL` 
4. **Null checks**: All functions validate parameters and return `TIMER_ERR_NULL_PARAM` if NULL
5. **Cleanup order**: Call `timer_port_cleanup()` before program exit to ensure thread joins cleanly

## Implementation Files

- **Header**: [lib/port/timer_port.h](../../lib/port/timer_port.h) - C API declaration
- **Implementation**: [lib/port/timer_port.cpp](../../lib/port/timer_port.cpp) - Single C++11 implementation
- **Backend Integration**: [src/backend.c](../../src/backend.c) - Driver usage
- **Tests**: [tests/test_backend/test_backend_timer.cpp](../../tests/test_backend/test_backend_timer.cpp) - GoogleTest suite

## Historical Notes

- Originally used generic -1/0 return values for error handling
- Updated January 2026 to use platform-agnostic `timer_error_t` enum with specific error codes
- Added `timer_error_string()` for better diagnostics
- Fixed backend integration bug where timer was incorrectly restarted in callback (returned `TIMER_ERR_ALREADY_ACTIVE` on every heartbeat after the first)
- **February 2026**: Replaced three platform-specific implementations (win32_timer.c, posix_timer.c, fallback_timer.c) with single portable C++11 implementation (timer_port.cpp)
  - Reduced codebase from ~600 lines to ~220 lines (70% reduction)
  - Eliminated platform-specific #ifdef complexity
  - Improved timing precision with `std::chrono::steady_clock`
  - Better resource management via RAII (C++ destructors)
  - Fixed wakeup mechanism: changed from `SetEvent()` (which doesn't interrupt IOCP) to `PostQueuedCompletionStatus()` with special `WAKEUP_COMPLETION_KEY`
