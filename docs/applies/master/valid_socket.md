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
The second argument is the name of the socket function that being called, which is one of below:
- `"create"` when [socket_create()](../../efuns/socket_create.md) is called.
- `"bind"` when [socket_bind()](../../efuns/socket_bind.md) is called.
- `"listen"` when [socket_listen()](../../efuns/socket_listen.md) is called.
- `"accept"` when [socket_accept()](../../efuns/socket_accept.md) is called.
- `"connect"` when [socket_connect()](../../efuns/socket_connect.md) is called.
- `"write"` when [socket_write()](../../efuns/socket_write.md) is called.
- `"close"` when [socket_close()](../../efuns/socket_close.md) is called.
- `"release"` when [socket_release()](../../efuns/socket_release.md) is called.
- `"acquire"` when [socket_acquire()](../../efuns/socket_acquire.md) is called.

The third argument is an array of information.
The first element of the array (when applicable) is file descriptor being referenced.
The second element of the array is the owner of the socket (object).
The third element of the array is the address (string) of the remote end of the socket.
The fourth element of the array is the port number associated with the socket.
