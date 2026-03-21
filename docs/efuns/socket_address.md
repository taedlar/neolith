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

The returned value is a numeric IPv4 endpoint string. If a connection was initiated with a hostname through `socket_connect()` on a DNS-enabled build, `socket_address()` reports the resolved numeric peer address, not the original hostname.

## RETURN VALUE
socket_address() returns:

a string format address on success.

an empty string on failure.

## SEE ALSO
[socket_connect()](socket_connect.md), [socket_create()](socket_create.md), [resolve()](resolve.md),
[query_host_name()](query_host_name.md), [query_ip_name()](query_ip_name.md), [query_ip_number()](query_ip_number.md)
