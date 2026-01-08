# Timer Port API

## Overview

The timer port API provides a platform-agnostic interface for periodic timer callbacks in the Neolith driver. It abstracts three different timer implementations:

1. **Windows**: `CreateWaitableTimer()` with dedicated callback thread
2. **POSIX**: `timer_create()` with `SIGALRM` signal handler (librt)
3. **Fallback**: `pthread` thread sleeping in a loop

All implementations share a common interface defined in [lib/port/timer_port.h](../../lib/port/timer_port.h).

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

**Platform Behavior**:
- **Windows**: Creates unnamed event and waitable timer objects
- **POSIX**: No initialization needed (signal handler set up during start)
- **Fallback**: Initializes mutex

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

**Platform Behavior**:
- **Windows**: Creates timer thread, sets waitable timer with due time
- **POSIX**: Installs `SIGALRM` handler, creates `CLOCK_REALTIME` timer
- **Fallback**: Creates pthread that sleeps in loop

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

**Platform Behavior**:
- **Windows**: Cancels waitable timer, signals event, joins thread
- **POSIX**: Disarms timer (keeps signal handler installed)
- **Fallback**: Sets stop flag, joins thread

**Note**: Safe to call even if timer not running (idempotent).

## Data Structures

### timer_port_t

Platform-specific union containing timer state:

```c
typedef struct timer_port_t {
#ifdef _WIN32
    struct {
        HANDLE timer_handle;
        HANDLE timer_event;
        HANDLE timer_thread;
        volatile int active;
        uint64_t interval_us;
        timer_callback_t callback;
    } win32;
#elif defined(HAVE_LIBRT)
    struct {
        timer_t timer_id;
        volatile int active;
        timer_callback_t callback;
    } posix;
#else
    struct {
        pthread_t thread;
        pthread_mutex_t mutex;
        volatile int active;
        uint64_t interval_us;
        timer_callback_t callback;
    } fallback;
#endif
} timer_port_t;
```

### timer_callback_t

```c
typedef void (*timer_callback_t)(void);
```

Callback function signature. Called from:
- **Windows**: Timer thread context
- **POSIX**: Signal handler context (restrictions apply)
- **Fallback**: Timer thread context

**Signal Safety (POSIX)**: Callback executes in signal handler, so it must only call async-signal-safe functions. In Neolith, the callback just sets `heart_beat_flag = 1`.

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
}
```

The callback simply sets a flag checked by the main event loop. The actual heartbeat processing happens in `call_heart_beat()`, which does NOT restart the timer.

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

## Platform Selection

Build system selects implementation at configure time:

1. **Windows** (`_WIN32`): Always uses Win32 timer
2. **Linux/Unix with librt** (`HAVE_LIBRT`): Uses POSIX timer
3. **Fallback**: Any POSIX system without librt (pthread only)

See [lib/port/CMakeLists.txt](../../lib/port/CMakeLists.txt) for selection logic.

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
2. **POSIX signal safety**: Callback must only use async-signal-safe functions
3. **Zero interval**: Returns `TIMER_ERR_INVALID_INTERVAL` (validation added in all implementations)
4. **Null checks**: All functions validate parameters and return `TIMER_ERR_NULL_PARAM` if NULL
5. **Thread safety**: Windows/fallback use threads; POSIX uses signals. Callback should only set atomic flags.

## Implementation Files

- **Header**: [lib/port/timer_port.h](../../lib/port/timer_port.h)
- **Windows**: [lib/port/win32_timer.c](../../lib/port/win32_timer.c)
- **POSIX**: [lib/port/posix_timer.c](../../lib/port/posix_timer.c)
- **Fallback**: [lib/port/fallback_timer.c](../../lib/port/fallback_timer.c)
- **Backend Integration**: [src/backend.c](../../src/backend.c)
- **Tests**: [tests/test_backend/test_backend_timer.cpp](../../tests/test_backend/test_backend_timer.cpp)

## Historical Notes

- Originally used generic -1/0 return values for error handling
- Updated January 2026 to use platform-agnostic `timer_error_t` enum with specific error codes
- Added `timer_error_string()` for better diagnostics
- Fixed backend integration bug where timer was incorrectly restarted in callback (returned `TIMER_ERR_ALREADY_ACTIVE` on every heartbeat after the first)
