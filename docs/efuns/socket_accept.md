# socket_accept()
## NAME
**socket_accept** - accept a connection on a socket

## SYNOPSIS
~~~cxx
#include <socket_err.h>
~~~

int socket_accept( int s, string read_callback,
string write_callback );

## DESCRIPTION
The argument s is a socket that has been created with
[socket_create()](socket_create.md), bound to an address with [socket_bind()](socket_bind.md),
and is listening for connections after a [socket_listen()](socket_listen.md).
socket_accept() extracts the first connection on the queue
of pending connections, creates a new socket with the same
properties of s and allocates a new file descriptor for the
socket. If no pending connections are present on the queue,
socket_accept() returns an error as described below. The
accepted socket is used to read and write data to and from
the socket which connected to this one; it is not used to
accept more connections. The original socket s remains open
for accepting further connections.

The argument read_callback is the name of a function for the
driver to call when the new socket (not the accepting
socket) receives data.  The write callback should follow
this format:

void read_callback(int fd)

Where fd is the socket which is ready to accept data.

The argument write_callback is the name of a function for
the driver to call when the new socket (not the accepting
socket) is ready to be written to. The write callback should
follow this format:

void write_callback(int fd)

Where fd is the socket which is ready to be written to.

Note: The close_callback of the accepting socket (not the
new socket) is called if the new socket closes unexpectedly,
i.e. not as the result of a [socket_close()](socket_close.md) call. The close
callback should follow this format:

void close_callback(int fd)

Where fd is the socket which has closed.

## RETURN VALUE
socket_accept() returns a non-negative descriptor for the
accepted socket on success. On failure, it returns a
negative value. [socket_error()](socket_error.md) can be used on the return
value to get a text description of the error.

## ERRORS
EEFDRANGE      Descriptor out of range.

EEBADF         Descriptor is invalid.

EESECURITY     Security violation attempted.

EEMODENOTSUPP  Socket mode not supported.

EENOTLISTN     Socket not listening.

EEWOULDBLOCK   Operation would block.

EEINTR         Interrupted system call.

EEACCEPT       Problem with accept.

EENOSOCKS      No more available efun sockets.

## SEE ALSO
[socket_bind()](socket_bind.md), [socket_connect()](socket_connect.md), [socket_create()](socket_create.md),
[socket_listen()](socket_listen.md)
