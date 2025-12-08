# Console Mode
This is a Neolith extension that treats standard input/output as a connecting user.
It allows using the Neolith LPMud Driver **as a shell** that runs code written in LPC.

- Simul efun object and master object are loaded as like in ordinary LPMud.
- Epilog and preload master apply works the same.
- Creation of the listen port(s) for incoming connection.
- If console mode is enabled, the driver calls master object's `conncet()` to connect the console user.
- Run the backend loop to do I/O multiplexing and command processing.
- If console user is disconnected, press ENTER to reconnect the console user.

## Winsock Limitations
On Windows, winsock does not support using standard inpuit (`STDIN_FILENO` or 0) in the read / write / except fd det.
It returns `WSAENOTSOCK` and refuses to do I/O multiplexing on the standard input file descriptor.
This limitation applies to `select()` and newer `WSAPoll()`.

A possible solution is to use [`WSAEventSelect()`](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaeventselect) to associate an event (system object) with the socket.
The backend loop can then wait for the standard input and all the other sockets with [`WSAWaitForMultipleEvents()`](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsawaitformultipleevents) which is implemented with `WaitForMultipleObjectsEx()` system call.
