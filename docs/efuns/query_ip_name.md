# query_ip_name
## NAME
          query_ip_name() - return the ip name of a given player
          object.

## SYNOPSIS
          string query_ip_name( object ob );

## DESCRIPTION
          Return the IP address for player `ob'.  An asynchronous
          process `addr_server' is used to find out these name in
          parallel.  If there are any failures to find the ip-name,
          then the ip-number is returned instead.

## SEE ALSO
          query_ip_number(3), query_host_name(3), resolve(3),
          socket_address(3)
