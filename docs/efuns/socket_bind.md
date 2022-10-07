# socket_bind
## NAME
          socket_bind() - bind a name to a socket

## SYNOPSIS
          #include <socket_err.h>

          int socket_bind( int s, int port );

## DESCRIPTION
          socket_bind() assigns a name to an unnamed socket. When a
          socket is created with socket_create(3) it exists in a name
          space (address family) but has no name assigned.
          socket_bind() requests that the port be assigned to the
          socket s.

## RETURN VALUE
          socket_bind() returns:

               EESUCCESS on success.

               a negative value indicated below on error.

## ERRORS
          EEFDRANGE      Descriptor out of range.

          EEBADF         Descriptor is invalid.

          EESECURITY     Security violation attempted.

          EEISBOUND      Socket is already bound.

          EEADDRINUSE    Address already in use.

          EEBIND         Problem with bind.

## NAME

## SEE ALSO
          socket_connect(3), socket_create(3), socket_listen(3)
