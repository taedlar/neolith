# valid_socket()
## NAME
**valid_socket** - protects the socket efuns

## SYNOPSIS
~~~cxx
int valid_socket (object caller, string function, mixed *info);
~~~

## DESCRIPTION
Each of the socket efuns calls `valid_socket()` prior to executing.
If `valid_socket()` returns 0, then the socket efun fails.
If `valid_socket()` returns 1, then the socket efun attempts to succeed.
The first argument **caller** is the object that called the socket efun.
The second argument is the name of the socket efun that is being called (e.g. `socket_write()` or `socket_bind()`).
The third argument is an array of information.
The first element of the array (when applicable) is file descriptor being referenced.
The second element of the array is the owner of the socket (object).
The third element of the array is the address (string) of the remote end of the socket.
The fourth element of the array is the port number associated with the socket.
