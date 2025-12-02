# Timer Port - Cross-Platform Timer Implementation

This directory contains a cross-platform timer abstraction layer that provides high-resolution periodic timers for the neolith MUD engine. The implementation automatically selects the best available timer mechanism based on the target platform.

## Implementation Overview

### Supported Platforms

1. **Windows (waitable_timer.c)** - Uses Windows waitable timers with dedicated timer threads
2. **POSIX with librt (posix_timer.c)** - Uses POSIX realtime timers with signals  
3. **Fallback (fallback_timer.c)** - Uses pthread-based polling for systems without native timer support

### Architecture

The timer port provides a unified API (`timer_port.h`) that abstracts platform differences:

```c
// Initialize timer system
int timer_port_init(timer_port_t *timer);

// Start periodic timer with callback
int timer_port_start(timer_port_t *timer, unsigned long interval_us, timer_callback_t callback);

// Stop timer
int timer_port_stop(timer_port_t *timer);

// Cleanup resources
void timer_port_cleanup(timer_port_t *timer);

// Check if timer is active
int timer_port_is_active(const timer_port_t *timer);
```

## Windows Implementation (win32_timer.c)

### Features
- **Waitable Timers**: Uses `CreateWaitableTimer()` and `SetWaitableTimer()` for high precision
- **Dedicated Thread**: Timer runs in separate thread to avoid blocking main execution
- **Thread Safety**: Proper synchronization between timer thread and main application
- **Resource Management**: Automatic cleanup of handles and threads

### Precision
- Theoretical resolution: 100ns (Windows timer resolution)
- Practical resolution: ~1ms (depends on system timer resolution)
- Supports microsecond-level interval specification

### Threading Model
```
Main Thread                Timer Thread
     |                          |
  timer_port_start()            |
     |                     WaitForMultipleObjects()
     |                          |
  [continues execution]    [timer fires]
     |                          |
     |                    callback()
     |                          |
  timer_port_stop()        [thread exits]
```

## POSIX Implementation (posix_timer.c)

### Features
- **POSIX Realtime Timers**: Uses `timer_create()` with `CLOCK_REALTIME`
- **Signal-Based**: Timer expiration triggers `SIGALRM` signal
- **High Precision**: Nanosecond resolution timer specification
- **Standard Compliant**: Uses standard POSIX.1b realtime extensions

### Signal Handling
- Installs `SIGALRM` handler during timer start
- Restores previous signal handler on timer stop
- Uses `SA_RESTART` flag to minimize signal interruption impact

## Fallback Implementation (fallback_timer.c)

### Features
- **Pthread-Based**: Uses separate thread with `nanosleep()` for timing
- **Portable**: Works on any system with pthread support
- **Simple**: Basic polling loop without complex timer APIs

### Limitations
- **Lower Precision**: Limited by `nanosleep()` implementation and thread scheduling
- **Higher Overhead**: Thread context switching on each timer event
- **Drift Potential**: May accumulate timing errors over long periods

## Integration Example

### Original backend.c code:
```c
#ifdef HAVE_LIBRT
static timer_t hb_timerid = 0;

// In backend():
if (-1 == timer_create (CLOCK_REALTIME, NULL, &hb_timerid)) {
    debug_perror ("timer_create()", NULL);
    return;
}

// In call_heart_beat():
struct itimerspec itimer;
itimer.it_interval.tv_sec = HEARTBEAT_INTERVAL / 1000000;
itimer.it_interval.tv_nsec = (HEARTBEAT_INTERVAL % 1000000) * 1000;
if (-1 == timer_settime (hb_timerid, 0, &itimer, NULL)) {
    debug_perror ("timer_settime()", NULL);
    return;
}
#endif
```

### New timer_port code:
```c
#include "port/timer_port.h"

static timer_port_t heartbeat_timer;

static void heartbeat_callback(void) {
    heart_beat_flag = 1;
}

// In backend():
if (timer_port_init(&heartbeat_timer) != 0) {
    opt_warn(0, "Timer functions not available");
    return;
}

// In call_heart_beat():
if (timer_port_start(&heartbeat_timer, HEARTBEAT_INTERVAL, heartbeat_callback) != 0) {
    debug_perror("timer_port_start()", NULL);
    return;
}
```

## Build Configuration

### CMake Integration

The timer implementation is automatically selected based on platform:

```cmake
# In lib/port/CMakeLists.txt
set(port_SOURCES
    $<$<PLATFORM_ID:Windows>:win32_timer.c>
    $<$<AND:$<BOOL:${HAVE_LIBRT}>,$<NOT:$<PLATFORM_ID:Windows>>>:posix_timer.c>
    $<$<AND:$<NOT:$<BOOL:${HAVE_LIBRT}>>,$<NOT:$<PLATFORM_ID:Windows>>>:fallback_timer.c>
)
```

### Library Dependencies

- **Windows**: `kernel32.lib` (automatically linked)
- **POSIX**: `librt` (linked when `HAVE_LIBRT` is true)
- **Fallback**: `pthread` (linked via `find_package(Threads)`)

## Testing

A comprehensive test suite is provided in `test_timer_port.c`:

```bash
# Build and run tests
cmake --build . --target test_timer_port
./test_timer_port
```

### Test Coverage
- Basic timer operation (start/stop/cleanup)
- High-frequency timing (100ms intervals)  
- Timer restart functionality
- Callback execution verification
- Resource cleanup verification

## Performance Characteristics

| Implementation | Resolution | Accuracy | CPU Overhead | Memory |
|----------------|------------|----------|--------------|--------|
| Windows        | ~1ms       | High     | Low          | Low    |
| POSIX          | ~1µs       | High     | Very Low     | Minimal|
| Fallback       | ~10ms      | Medium   | Medium       | Low    |

## Migration Benefits

1. **Cross-Platform Compatibility**: Single codebase works on Windows, Linux, macOS, and other POSIX systems
2. **Cleaner Code**: Eliminates platform-specific `#ifdef` blocks in main application code
3. **Better Testability**: Unified interface enables comprehensive testing across platforms
4. **Maintainability**: Platform-specific timer logic is isolated and well-documented
5. **Performance**: Each implementation uses the optimal timer mechanism for its platform

## Future Enhancements

- **macOS Optimization**: Add `mach_timebase_info` for improved precision on macOS
- **Thread Pool**: Consider using thread pool for multiple timers
- **Timer Coalescing**: Implement timer coalescing for better power efficiency
- **Statistics**: Add timing accuracy measurement and drift detection
- **Priority Support**: Add timer priority levels for different timer types