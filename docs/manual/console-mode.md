# Console Mode

This is a Neolith extension that treats standard input/output as a connecting user.
It allows using the Neolith LPMud Driver **as a shell** that runs code written in LPC.

## Enabling Console Mode

Console mode is enabled via the `-c` or `--console-mode` command line option:

~~~sh
neolith -f neolith.conf -c
~~~

## How Console Mode Works

The console mode modifies the normal startup sequence:

1. Simul efun object and master object are loaded as in ordinary LPMud.
2. Epilog and preload master applies work the same.
3. Creation of the listen port(s) for incoming connections.
4. **If console mode is enabled**, the driver calls master object's `connect()` apply with port number = 0 to connect the console user.
5. Run the backend loop to do I/O multiplexing and command processing.

## Console Connection Behavior

The console connection is treated as an interactive user with special properties:

- After the master object finishes preloading, it receives a `connect()` apply with port number = 0
- The master object can then navigate the connection through regular [logon](../applies/interactive/logon.md) process of player object
- Despite not using TELNET protocol, the `input_to()` and `get_char()` efuns are supported for console connection
- When the console connection is closed (e.g., typing "quit" or forced by another wizard), the standard input is **NOT** closed and allows **reconnecting** to the MUD in console mode by pressing ENTER
- You can use Ctrl-C to **break** the LPMud driver process as with other processes reading data from standard input

## Use Cases

Console mode is particularly useful for:

1. **Testing and Development**: Quickly test LPC code without setting up network connections
2. **Debugging**: Run the driver under a debugger (like `gdb`) while interacting with it
3. **Automated Testing**: Run scripted tests by piping commands to the driver
4. **Server Management**: Perform administrative tasks without network access

## Testing Console Mode

See [Developer Manual](dev.md) for build and configuration setup.

### Interactive Testing

**1. Start driver in console mode:**
~~~sh
# Linux/WSL
neolith -f neolith.conf -c

# Windows
neolith.exe -f neolith.conf -c
~~~

**2. Verify console connection:**
- Driver startup completes normally
- Console receives `connect()` apply with port number = 0
- Logon prompts appear (username/password)

**3. Test features:**
- Execute MUD commands
- Verify `input_to()` works for password prompts
- Test `get_char()` for single-character input (if mudlib uses it)
- Test Unicode input (e.g., "café", "日本語")

**4. Test reconnection:**
- Type "quit" or disconnect
- Press ENTER to reconnect without restarting driver
- Verify Ctrl+C interrupts the driver

**Example session:**
~~~sh
$ neolith -f neolith.conf -c
===== neolith version 1.0.0 starting up =====
...
Welcome to the MUD!
What is your name? admin
Password:
> say Hello from console!
You say: Hello from console!
> quit
Goodbye!
[Press ENTER to reconnect]
~~~

### Manual Command Line Testing

**Piped commands (cross-platform):**
~~~sh
# Linux/WSL
echo -e "admin\npassword\nsay test\nshutdown" | neolith -f neolith.conf -c

# Windows PowerShell
"admin`npassword`nsay test`nshutdown" | .\neolith.exe -f neolith.conf -c
~~~

**Redirected file input (cross-platform):**
~~~sh
# Linux/WSL
echo -e "admin\npassword\nsay test\nshutdown" > commands.txt
neolith -f neolith.conf -c < commands.txt

# Windows PowerShell
"admin`npassword`nsay test`nshutdown" | Out-File commands.txt
Get-Content commands.txt | .\neolith.exe -f neolith.conf -c
~~~

**Note**: When using piped or redirected input, the driver will process commands until the pipe closes (EOF), then automatically shutdown. This is different from interactive console mode where the driver waits for reconnection after user disconnects.

### Python Test Automation

The repository includes [testbot.py](../../examples/testbot.py) as a template for automated console mode testing on both platforms:

~~~sh
cd examples
python testbot.py
~~~

This testing robot demonstrates how to pipe test commands to the driver and validate output. It serves as a starting point for building more advanced test automation:
- Testing specific LPC functionality
- Regression testing after code changes
- Performance benchmarking
- Stress testing with concurrent operations

**Platform support:**
- ✅ **Linux/WSL**: Fully functional (pipes work with poll())
- ✅ **Windows**: Fully functional (synchronous ReadFile() for pipes)

**How it works:**
1. Sends commands via `subprocess.Popen` with piped stdin
2. Driver detects pipe (not real console) and preserves all input data
3. Commands are processed line-by-line until EOF
4. Driver automatically shuts down on pipe closure
5. Test validates exit code and output

See [Console Testbot Support](console-testbot-support.md) for design details.

### Unit Tests

The I/O reactor includes comprehensive console mode tests at the C++ level:

**POSIX** ([test_io_reactor_console.cpp](../../tests/test_io_reactor/test_io_reactor_console.cpp)):
- 7 test cases using pipes to simulate stdin
- Covers: basic input, network coexistence, EOF handling, large input

**Windows** ([test_io_reactor_console.cpp](../../tests/test_io_reactor/test_io_reactor_console.cpp)):
- 5 test cases for Windows console handling
- Covers: console registration, coexistence with sockets, non-blocking behavior

**Run tests:**
~~~sh
# Linux
ctest --preset ut-linux --tests-regex Console --output-on-failure

# Windows
ctest --preset ut-vs16-x64 --tests-regex Console --output-on-failure
~~~

See [Async Library Design](../internals/async-library.md) and [Console Worker Tests](../../tests/test_console_worker/) for complete test documentation.

### Troubleshooting

**Driver exits immediately:**
- Check `MudlibDir` path in config (must be absolute or relative to current directory)
- Verify `master.c` exists and compiles
- Check debug log for errors

**No input accepted:**
- Windows: Verify stdin is a real console (not piped/redirected)
- Check for "Failed to register console input" messages
- Look for `GetStdHandle(STD_INPUT_HANDLE)` errors

**Unicode issues:**
- Windows: Console should auto-configure for UTF-8
- Linux: Verify locale supports UTF-8 (`echo $LANG`)

**Expected warnings (normal):**
- "Console input does not support virtual terminal sequences" - informational only
- "no simul_efun file" - mudlib configuration, not a driver error

## Platform Implementation

Console mode is fully supported on both Linux and Windows through the I/O reactor abstraction layer.

### POSIX (Linux/WSL)

On POSIX systems, `STDIN_FILENO` (file descriptor 0) is registered directly with the reactor using `io_reactor_add()`. The reactor uses standard `poll()` to multiplex console input alongside network sockets.

### Windows Implementation

Windows console handles cannot be used with Winsock `select()` or I/O Completion Ports (IOCP). The reactor employs a platform-specific solution that supports both real consoles and piped stdin:

**Handle Type Detection**:
- Uses `GetFileType()` to distinguish console vs pipe vs file
- Real console (`FILE_TYPE_CHAR` + `GetConsoleMode()` succeeds): Uses `ReadConsoleInputW()`
- Pipe (`FILE_TYPE_PIPE`): Uses synchronous `ReadFile()`
- File (`FILE_TYPE_DISK`): Uses synchronous `ReadFile()`

**Real Console Mode**:
- `ReadConsoleInputW()` reads raw `INPUT_RECORD` structures (keyboard events)
- Extracts Unicode characters from `KEY_EVENT` records
- Converts UTF-16 to UTF-8 via `WideCharToMultiByte(CP_UTF8)`
- Non-blocking operation via `GetNumberOfConsoleInputEvents()` check
- Supports reconnection (stdin remains open after disconnect)

**Pipe/File Mode** (for automated testing):
- Synchronous `ReadFile()` on stdin handle
- Processes input line-by-line until EOF
- EOF triggers clean shutdown instead of reconnection
- Enables `testbot.py` and piped command automation

**Event Loop Integration**:
- `io_reactor_wait()` checks console availability before blocking on IOCP
- Real console: Polled via `GetNumberOfConsoleInputEvents()`
- Pipe/file: Always considered ready (synchronous read handles blocking)
- Returns `EVENT_READ` when console input available

**Design Benefits**:
- ✅ Full Unicode support (UTF-16 → UTF-8 conversion for real console)
- ✅ Cross-platform testbot.py support
- ✅ Non-blocking operation - no thread overhead
- ✅ Console and network I/O handled in unified event loop
- ✅ Automatic EOF handling (pipes exit cleanly, consoles reconnect)

See [Console Worker Implementation](../../lib/async/console_worker.c) for complete technical details and [Console Testbot Support](console-testbot-support.md) for design overview.
