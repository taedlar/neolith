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

## Example Session

~~~sh
$ neolith -f neolith.conf -c
2025-12-30 10:15:23     {}      ===== neolith version 0.1.0 starting up =====
2025-12-30 10:15:23     {}      using MudLibDir "/home/user/mudlib"
2025-12-30 10:15:23     {}      ----- loading simul efuns -----
2025-12-30 10:15:23     {}      ----- loading master -----
2025-12-30 10:15:23     {}      ----- epilogue -----
2025-12-30 10:15:23     {}      ----- entering MUD -----
Welcome to the MUD!
What is your name? admin
Password:
> say Hello from console!
You say: Hello from console!
> quit
Goodbye!
[Press ENTER to reconnect]
~~~

## Winsock Limitations
On Windows, winsock does not support using standard inpuit (`STDIN_FILENO` or 0) in the read / write / except fd det.
It returns `WSAENOTSOCK` and refuses to do I/O multiplexing on the standard input file descriptor.
This limitation applies to `select()` and newer `WSAPoll()`.

A possible solution is to use [`WSAEventSelect()`](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaeventselect) to associate an event (system object) with the socket.
The backend loop can then wait for the standard input and all the other sockets with [`WSAWaitForMultipleEvents()`](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsawaitformultipleevents) which is implemented with `WaitForMultipleObjectsEx()` system call.
