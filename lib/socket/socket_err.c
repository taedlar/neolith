/*  $Id: socket_err.c,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

/*
 * socket_errors.c -- socket error strings
 *    5-92 : Dwayne Fontenot (Jacques@TMI) : original coding.
 *   10-92 : Dave Richards (Cynosure) : less original coding.
 */

#include "LPC/socket_err.h"

char *error_strings[ERROR_STRINGS] = {
  "Problem creating socket",
  "Problem with setsockopt",
  "Problem setting non-blocking mode",
  "No more available efun sockets",
  "Descriptor out of range",
  "Socket is closed",
  "Security violation attempted",
  "Socket is already bound",
  "Address already in use",
  "Problem with bind",
  "Problem with getsockname",
  "Socket mode not supported",
  "Socket not bound to an address",
  "Socket is already connected",
  "Problem with listen",
  "Socket not listening",
  "Operation would block",
  "Interrupted system call",
  "Problem with accept",
  "Socket is listening",
  "Problem with address format",
  "Operation already in progress",
  "Connection refused",
  "Problem with connect",
  "Socket not connected",
  "Object type not supported",
  "Problem with sendto",
  "Problem with send",
  "Wait for callback",
  "Socket already released",
  "Socket not released",
  "Data nested too deeply"
};
