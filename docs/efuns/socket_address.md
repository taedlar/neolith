# socket_address()
## NAME
**socket_address** - return the remote address for an efun
socket

## SYNOPSIS
~~~cxx
#include <socket_err.h>
~~~

string socket_address( int s );

## DESCRIPTION
socket_address() returns the remote address for an efun
socket s.  The returned address is of the form:

"127.0.0.1 23".

## RETURN VALUE
socket_address() returns:

a string format address on success.

an empty string on failure.

## SEE ALSO
[socket_connect()](socket_connect.md), [socket_create()](socket_create.md), [resolve()](resolve.md),
[query_host_name()](query_host_name.md), [query_ip_name()](query_ip_name.md), [query_ip_number()](query_ip_number.md)
