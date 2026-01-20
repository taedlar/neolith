# Testing Robot

## Overview

Neolith supports automated testing via piped stdin on both Linux/WSL and Windows platforms. This enables `testbot.py` and similar automation scripts to send commands to the driver non-interactively, making regression testing and CI/CD integration possible.

## Design Principle

The implementation uses a **hybrid dispatch system**:

- **Real Terminals** (TTY/Console): Use platform-specific APIs for optimal UX and security
- **Pipes/Files** (Testing Robots): Use generic I/O to preserve all input data for automation

This ensures that:
- ✅ Interactive console mode works optimally for human users
- ✅ Automated testing works reliably on both platforms
- ✅ Security is preserved (terminals flush input on mode changes)
- ✅ Backward compatibility is maintained

## Platform Implementation

### POSIX (Linux/WSL)

**Detection**: Uses `isatty(fd)` to distinguish TTY from pipes/files

**Behavior**:
- **Real TTY**: Uses `tcsetattr(..., TCSAFLUSH, ...)` to flush input on mode changes (security)
- **Pipe/File**: Uses `tcsetattr(..., TCSANOW, ...)` to preserve all input data (automation)

**Implementation**: `safe_tcsetattr()` helper function conditionally applies flushing based on `isatty()` result

### Windows

**Detection**: Uses `GetFileType(handle)` to distinguish console from pipes/files

**Behavior**:
- **Real Console**: Uses `ReadConsoleInputW()` for Unicode keyboard events (existing implementation)
- **Pipe**: Uses synchronous `ReadFile()` for simple data streaming
- **File**: Uses synchronous `ReadFile()` for file input

**Implementation**: `console_type_t` enum tracks handle type, dispatcher selects appropriate I/O method

## EOF Handling

When stdin reaches EOF:

- **Real Console/TTY**: Display reconnection prompt (existing behavior)
- **Pipe/File**: Call `do_shutdown(0)` for clean exit (new behavior)

This distinction enables automated tests to exit cleanly while preserving interactive console functionality.

## Usage

### Automated Testing

```bash
# Linux/WSL
echo -e "say test\nshutdown" | neolith -f config.conf -c

# Windows
"say test`nshutdown" | neolith.exe -f config.conf -c

# Python testbot
cd examples
python testbot.py
```

### Testing Strategy

See [examples/README.md](../../examples/README.md) for comprehensive testing guide including:
- Interactive testing procedures
- Piped command examples
- File redirect patterns
- Troubleshooting tips

## Platform Support Matrix

| Platform | Real Console | Piped Stdin | File Redirect |
|----------|--------------|-------------|---------------|
| **Linux/WSL** | ✅ Works | ✅ Works | ✅ Works |
| **Windows** | ✅ Works | ✅ Works | ✅ Works |

All modes preserve optimal UX for their respective use cases.

## Security Considerations

**Input Flushing**:
- Real terminals flush input on mode changes (prevents injection attacks)
- Pipes preserve all data (controlled by test author)

**Attack Surface**: Low - piped input implies trusted source (developer's own test scripts)

**Mitigation**: Console mode is for development/testing only, not production use

## Performance Impact

- **POSIX**: `isatty()` adds ~1µs per mode switch (4 calls per session) - negligible
- **Windows**: `GetFileType()` adds ~1µs at startup - negligible
- Pipe I/O uses same patterns as network sockets - already optimized

## Backward Compatibility

✅ **Fully backward compatible**:
- No API, config, or LPC changes
- Real terminal behavior unchanged
- Existing tests continue to pass
- New functionality for previously unsupported scenarios

## Implementation Details

For detailed implementation information, see:

- **Development Plan**: [docs/history/console-testbot-support.md](../history/console-testbot-support.md) (archived - completed)
- **Phase 1 (POSIX)**: [docs/history/agent-reports/console-testbot-phase1.md](../history/agent-reports/console-testbot-phase1.md) (if exists)
- **Phase 2 (Windows)**: [docs/history/agent-reports/console-testbot-phase2.md](../history/agent-reports/console-testbot-phase2.md)
- **Phase 3 (Documentation)**: [docs/history/agent-reports/console-testbot-phase3-complete.md](../history/agent-reports/console-testbot-phase3-complete.md)

## Related Documentation

- [Console Mode Manual](console-mode.md) - Complete console mode documentation
- [Async Library Design](../internals/async-library.md) - Unified async runtime architecture
- [Console Worker Implementation](../../lib/async/console_worker.c) - Platform-specific console handling
- [Examples Guide](../../examples/README.md) - Testing procedures and troubleshooting
- [testbot.py](../../examples/testbot.py) - Testing robot template
