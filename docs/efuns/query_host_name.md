# query_host_name
## NAME
query_host_name() - return the host name

## SYNOPSIS
~~~cxx
string query_host_name( void );
~~~

## DESCRIPTION
query_host_name() returns the name of the host.

## RETURN VALUE
query_host_name() returns:

- a string hostname on success.
- an empty string on failure.

## SEE ALSO
[resolve()](resolve.md),
[socket_address()](socket_address.md),
[query_ip_name()](query_ip_name.md),
[query_ip_number()](query_ip_number.md)
