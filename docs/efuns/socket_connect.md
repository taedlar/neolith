# socket_connect()
## NAME
**socket_connect** - initiate a connection on a socket

## SYNOPSIS
~~~cxx
#include <socket_err.h>

int socket_connect( int s, string address, string read_callback, string write_callback );
~~~

## DESCRIPTION
The argument **s** is a socket.
**s** must be either a `STREAM` mode or a `MUD` mode socket.
**address** is the address to which the socket will attempt to connect.
**address** is of the form:

"127.0.0.1 23"

The argument **read_callback** is the name of a function for the driver to call when the socket gets data from its peer.
The read callback should follow this format:
~~~cxx
void read_callback(int fd, mixed message)
~~~
Where **fd** is the socket which received the data, and **message** is the data which was received.

The argument **write_callback** is the name of a function for the driver to call when the socket is ready to be written to.
The write callback should follow this format:
~~~cxx
void write_callback(int fd)
~~~
Where **fd** is the socket which is ready to be written to.

## RETURN VALUE
socket_connect() returns:

`EESUCCESS` on success.

A negative value indicated below on error.

## ERRORS
Error Code|Description
---|---
`EEFDRANGE`|Descriptor out of range.
`EEBADF`|Descriptor is invalid.
`EESECURITY`|Security violation attempted.
`EEMODENOTSUPP`|Socket mode not supported.
`EEISLISTEN`|Socket is listening.
`EEISCONN`|Socket is already connected.
`EEBADADDR`|Problem with address format.
`EEINTR`|Interrupted system call.
`EEADDRINUSE`|Address already in use.
`EEALREADY`|Operation already in progress.
`EECONNREFUSED`|Connection refused.
`EECONNECT`|Problem with connect.

## SEE ALSO
[socket_accept()](socket_accept.md),
[socket_close()](socket_close.md),
[socket_create()](socket_create.md)
