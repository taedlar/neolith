# socket_write()
## NAME
**socket_write** - send a message from a socket

## SYNOPSIS
~~~cxx
#include <socket_err.h>
~~~

int socket_write( int s, mixed message,
void | string address );

## DESCRIPTION
socket_write() sends a message on a socket s. If the socket
s is of type STREAM or MUD, the socket must already be
connected and the address is not specified. If the socket is
of type DATAGRAM, the address must be specified.  The
address is of the form: "127.0.0.1 23".

Address-format notes:
- DATAGRAM addresses are currently numeric IPv4 endpoints.
- Unlike `socket_connect()`, `socket_write()` does not perform built-in hostname resolution.
- If mudlib code needs to send to a hostname, resolve the hostname in LPC first and then call `socket_write()` with the numeric IPv4 endpoint. See [lpc-dns-resolver](../manual/lpc-dns-resolver.md).

## RETURN VALUE
socket_write() returns:

EESUCCESS on success.

a negative value indicated below on error.

## ERRORS
EEFDRANGE      Descriptor out of range.

EEBADF         Descriptor is invalid.

EESECURITY     Security violation attempted.

EENOADDR       Socket not bound to an address.

EEBADADDR      Problem with address format.

EENOTCONN      Socket not connected.

EEALREADY      Operation already in progress.

EETYPENOTSUPP  Object type not supported.

EEBADDATA      Sending data with too many nested levels.

EESENDTO       Problem with sendto.

EEMODENOTSUPP  Socket mode not supported.

EEWOULDBLOCK   Operation would block.

EESEND         Problem with send.

EECALLBACK     Wait for callback.

## SEE ALSO
[socket_connect()](socket_connect.md), [socket_create()](socket_create.md), [lpc-dns-resolver](../manual/lpc-dns-resolver.md)
