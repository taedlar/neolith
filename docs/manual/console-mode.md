# Console Mode

Console mode is a Neolith extension that treats standard input and output as a connected interactive user.
It is mainly used for local development, debugging, and scripted testing.

## Enabling Console Mode

Use `-c` (or `--console-mode`) when starting the driver:

~~~sh
neolith -f neolith.conf -c
~~~

## Connection Lifecycle

When console mode is enabled:

1. The normal startup path runs (master load, epilog, preload).
2. The backend initializes runtime I/O.
3. The driver creates a console interactive and calls master `connect(0)`.
4. If accepted by mudlib `connect()`, normal `logon()` flow continues.

Console user behavior:

- Console connection is interactive but does not use TELNET protocol.
- `input_to()` and `get_char()` are supported.
- If the console user disconnects in a real terminal session, stdin stays open and ENTER can reconnect.
- Ctrl-C handling follows normal terminal behavior.

## Current Design

Console mode uses async runtime plus a dedicated console worker.

### Runtime Architecture

- Backend creates `async_runtime_t`.
- Network descriptors are registered with async runtime.
- Console input is handled by `console_worker` (worker thread).
- Worker pushes completed lines to `async_queue_t`.
- Worker posts completions (`CONSOLE_COMPLETION_KEY`) to wake backend.
- Backend drains queued lines into the console interactive command buffer.

This keeps command processing unified while avoiding platform-specific stdin blocking in the backend thread.

### Console Type Detection

Console worker detects stdin type at startup:

- `CONSOLE_TYPE_REAL`: interactive terminal or console.
- `CONSOLE_TYPE_PIPE`: piped stdin.
- `CONSOLE_TYPE_FILE`: redirected file stdin.
- `CONSOLE_TYPE_NONE`: no usable stdin.

Detection uses:

- Windows: `GetConsoleMode()` and `GetFileType()`.
- POSIX: `isatty()` and `fstat()`.

### Echo and Single-Character Mode

POSIX builds with termios use termios for echo and single-char mode.

Windows builds use shared helpers in [lib/async/console_mode.h](../../lib/async/console_mode.h):

- `set_console_input_line_mode(echo)`
- `set_console_input_echo(echo)`
- `set_console_input_single_char(single)`
- `enable_console_output_ansi()`

These helpers are used by:

- Console worker startup.
- Console reconnect path in backend.
- `set_console_echo()`.
- Console branch of `set_telnet_single_char()`.

For pipe and file stdin, helper calls are no-op by design, preserving scripted input behavior.

### EOF and Reconnection

- Real console disconnect: reconnect prompt path remains available.
- Pipe or file EOF: driver proceeds to shutdown.

This split supports both interactive use and deterministic automation.

## Testing Console Mode

See [docs/manual/dev.md](dev.md) for build and configuration setup.

### Interactive Smoke Test

~~~sh
# Linux or WSL
neolith -f neolith.conf -c

# Windows
neolith.exe -f neolith.conf -c
~~~

Quick checks:

1. Startup reaches logon prompt via `connect(0)`.
2. Normal commands execute.
3. `input_to()` no-echo prompts work.
4. `get_char()` paths work (if mudlib uses them).
5. Disconnect and press ENTER to verify reconnect (real terminal only).

### Scripted Input Tests

Piped input:

~~~sh
# Linux or WSL
echo -e "admin\npassword\nsay test\nshutdown" | neolith -f neolith.conf -c

# Windows PowerShell
"admin`npassword`nsay test`nshutdown" | .\neolith.exe -f neolith.conf -c
~~~

Redirected input:

~~~sh
# Linux or WSL
echo -e "admin\npassword\nsay test\nshutdown" > commands.txt
neolith -f neolith.conf -c < commands.txt

# Windows PowerShell
"admin`npassword`nsay test`nshutdown" | Out-File commands.txt
Get-Content commands.txt | .\neolith.exe -f neolith.conf -c
~~~

The driver processes commands until EOF and then shuts down for pipe or file modes.

### Python Automation

Use [examples/testbot.py](../../examples/testbot.py):

~~~sh
cd examples
python testbot.py
~~~

Also see [docs/manual/testbot.md](testbot.md) for broader automation guidance.

## Unit Tests

Current console-mode coverage is in [tests/test_console_worker](../../tests/test_console_worker):

- [tests/test_console_worker/test_console_worker_detection.cpp](../../tests/test_console_worker/test_console_worker_detection.cpp)
- [tests/test_console_worker/test_console_worker_lifecycle.cpp](../../tests/test_console_worker/test_console_worker_lifecycle.cpp)
- [tests/test_console_worker/test_async_runtime_console.cpp](../../tests/test_console_worker/test_async_runtime_console.cpp)

Related interactive behavior coverage (input flags and single-char flow):

- [tests/test_lpc_interpreter/test_input_to_get_char.cpp](../../tests/test_lpc_interpreter/test_input_to_get_char.cpp)

Run focused tests:

~~~sh
# Linux
ctest --preset ut-linux -R "ConsoleWorker|AsyncRuntimeConsole|InputToGetChar"

# Windows
ctest --preset ut-vs16-x64 -R "ConsoleWorker|AsyncRuntimeConsole|InputToGetChar"
~~~

## Troubleshooting

Driver exits immediately:

- Verify mudlib path and config values.
- Verify master object compiles.
- Check startup logs.

No input accepted:

- Confirm stdin mode (real console vs pipe/file).
- Check console worker initialization logs.
- On Windows, check console handle availability from `GetStdHandle(STD_INPUT_HANDLE)`.

Unicode issues:

- Windows console path configures UTF-8 output mode for real consoles.
- On POSIX, verify locale settings.

## References

- [docs/internals/async-library.md](../internals/async-library.md)
- [lib/async/console_worker.c](../../lib/async/console_worker.c)
- [lib/async/console_mode.c](../../lib/async/console_mode.c)
- [docs/manual/testbot.md](testbot.md)
