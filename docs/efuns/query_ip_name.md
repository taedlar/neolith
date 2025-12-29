# query_ip_name()
## NAME
**query_ip_name** - return the ip name of a given player
object.

## SYNOPSIS
~~~cxx
string query_ip_name( object ob );
~~~

## DESCRIPTION
Return the IP address for player `ob'.  An asynchronous
process `addr_server' is used to find out these name in
parallel.  If there are any failures to find the ip-name,
then the ip-number is returned instead.

## SEE ALSO
[query_ip_number()](query_ip_number.md), [query_host_name()](query_host_name.md), [resolve()](resolve.md),
[socket_address()](socket_address.md)
