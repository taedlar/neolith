/*  $Id: socket_ctrl.c,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include "src/main.h"

/*
  ioctl.c: part of the MudOS release -- Truilkan@TMI

  isolates the code which sets the various socket modes since various
  machines seem to need this done in different ways.
*/

/*
 * set process receiving SIGIO/SIGURG signals to us.
 */

int
set_socket_owner (int fd, int which)
{
#ifdef OLD_ULTRIX
  return fcntl (fd, F_SETOWN, which);
#else
#ifdef WINSOCK
  return 1;			/* FIXME */
#else
  return ioctl (fd, SIOCSPGRP, &which);
#endif
#endif
}

/*
 * allow receipt of asynchronous I/O signals.
 */

int
set_socket_async (int fd, int which)
{
#ifdef OLD_ULTRIX
  return fcntl (fd, F_SETFL, FASYNC);
#else
  return ioctl (fd, FIOASYNC, &which);
#endif
}

/*
 * set socket non-blocking
 */

int
set_socket_nonblocking (int fd, int which)
{
#if !defined(OLD_ULTRIX) && !defined(_SEQUENT_)
  int result;
#endif

#ifdef OLD_ULTRIX
  if (which)
    return fcntl (fd, F_SETFL, FNDELAY);
  else
    return fcntl (fd, F_SETFL, FNBLOCK);
#else

#ifdef _SEQUENT_
  int flags = fcntl (fd, F_GETFL, 0);

  if (flags == -1)
    return (-1);
  if (which)
    flags |= O_NONBLOCK;
  else
    flags &= ~O_NONBLOCK;
  return fcntl (fd, F_SETFL, flags);
#else
  result = ioctl (fd, FIONBIO, &which);
  if (result == -1)
    debug_perror ("set_socket_nonblocking: ioctl", 0);
  return result;
#endif

#endif
}
