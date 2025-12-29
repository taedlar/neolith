# socket_close()
## NAME
**socket_close** - close a socket

## SYNOPSIS
~~~cxx
#include <socket_err.h>
~~~

int socket_close( int s );

## DESCRIPTION
socket_close() closes socket s. This frees a socket efun
slot for use.

## RETURN VALUE
socket_close() returns:

EESUCCESS on success.

a negative value indicated below on error.

## ERRORS
EEFDRANGE      Descriptor out of range.

EEBADF         Descriptor is invalid.

EESECURITY     Security violation attempted.

## SEE ALSO
[socket_accept()](socket_accept.md), [socket_create()](socket_create.md)
