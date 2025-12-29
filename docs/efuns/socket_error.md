# socket_error()
## NAME
**socket_error** - return a text description of a socket error

## SYNOPSIS
~~~cxx
#include <socket_err.h>
~~~

string socket_error( int error );

## DESCRIPTION
socket_error() returns a string describing the error
signified by error.

## RETURN VALUE
socket_error() returns:

a string describing the error on success.

"socket_error: invalid error number" on bad input.

## SEE ALSO
[socket_create()](socket_create.md), [socket_connect()](socket_connect.md)
