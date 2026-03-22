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
**address** is the endpoint to which the socket will attempt to connect.

Original MudOS behavior accepted only numeric IPv4 endpoints. The baseline compatible form remains:

"127.0.0.1 23"

Current Neolith behavior:
- Numeric IPv4 endpoints remain supported and unchanged.
- Hostname endpoints such as `"localhost 23"` are also accepted when the driver is built with `PACKAGE_SOCKET_CONNECT_DNS` enabled.
- When built-in DNS support is disabled, hostname endpoints fail fast with `EEBADADDR`.

When hostname support is enabled, `socket_connect()` queues DNS resolution asynchronously and returns without blocking the backend loop. After resolution, the socket continues through the normal non-blocking connect path.

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

For portable mudlib code that must work on both DNS-enabled and DNS-disabled builds, resolve hostnames in LPC first and then call `socket_connect()` with a numeric IPv4 endpoint. See [lpc-dns-resolver](../manual/lpc-dns-resolver.md).

## RETURN VALUE
socket_connect() returns:

`EESUCCESS` on success.

A negative value indicated below on error.

Notes:
- For numeric IPv4 endpoints, `EESUCCESS` means the connect path was accepted successfully.
- For hostname endpoints with built-in DNS enabled, `EESUCCESS` means DNS work was admitted successfully; final connection success or failure is completed asynchronously.
- Under built-in DNS admission-control pressure, hostname connects may return `EEWOULDBLOCK`.

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
`EEWOULDBLOCK`|Built-in DNS admission control rejected the hostname lookup because the DNS system is at capacity.

## SEE ALSO
[socket_accept()](socket_accept.md),
[socket_close()](socket_close.md),
[socket_create()](socket_create.md),
[socket_write()](socket_write.md),
[lpc-dns-resolver](../manual/lpc-dns-resolver.md)
