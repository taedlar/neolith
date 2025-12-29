# socket_create()
## NAME
**socket_create** - create an efun socket

## SYNOPSIS
~~~cxx
#include <socket_err.h>
~~~

int socket_create( int mode, string read_callback,
void | string close_callback );

## DESCRIPTION
socket_create() creates an efun socket. mode determines
which type of socket is created. Currently supported socket
modes are:

MUD         for sending LPC data types using TCP protocol.

STREAM      for sending raw data using TCP protocol.

DATAGRAM    for using UDP protocol.

The argument read_callback is the name of a function for the
driver to call when the socket gets data from its peer. The
read callback should follow this format:

void read_callback(int fd, mixed message)

Where fd is the socket which received the data, and message
is the data which was received.

The argument close_callback is the name of a function for
the driver to call if the socket closes unexpectedly, i.e.
not as the result of a [socket_close()](socket_close.md) call. The close
callback should follow this format:

void close_callback(int fd)

Where fd is the socket which has closed.  NOTE:
close_callback is not used with DATAGRAM mode sockets.

## RETURN VALUE
socket_create() returns:

a non-negative descriptor on success.

a negative value indicated below on error.

## ERRORS
EEMODENOTSUPP  Socket mode not supported.

EESOCKET       Problem creating socket.

EESETSOCKOPT   Problem with setsockopt.

EENONBLOCK     Problem setting non-blocking mode.

EENOSOCKS      No more available efun sockets.

EESECURITY     Security violation attempted.

## SEE ALSO
[socket_accept()](socket_accept.md), [socket_bind()](socket_bind.md), [socket_close()](socket_close.md),
[socket_connect()](socket_connect.md), [socket_listen()](socket_listen.md), [socket_write()](socket_write.md)
