#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include "port/socket_comm.h"
#include "src/std.h"

/*
  ioctl.c: part of the MudOS release -- Truilkan@TMI

  isolates the code which sets the various socket modes since various
  machines seem to need this done in different ways.
*/

/**
 *  @brief Set process receiving SIGIO/SIGURG signals to us.
 */
int set_socket_owner (int fd, int which)
{
#ifdef OLD_ULTRIX
  return fcntl (fd, F_SETOWN, which);
#elif defined(WINSOCK)
  (void)fd;
  (void)which;
  return 1; /* No equivalent */
#else
  return ioctl (fd, SIOCSPGRP, &which);
#endif
}

/**
 *  @brief Allow receipt of asynchronous I/O signals.
 */
int set_socket_async (int fd, int which)
{
#ifdef OLD_ULTRIX
  return fcntl (fd, F_SETFL, FASYNC);
#elif defined(WINSOCK)
  (void)fd;
  (void)which;
  return 1; /* No equivalent */
#else
  return ioctl (fd, FIOASYNC, &which);
#endif
}

/**
 *  @brief Set socket non-blocking
 */
int set_socket_nonblocking (int fd, int which)
{
#if !defined(OLD_ULTRIX) && !defined(_SEQUENT_)
  int result;
#endif

#ifdef OLD_ULTRIX
  if (which)
    return fcntl (fd, F_SETFL, FNDELAY);
  else
    return fcntl (fd, F_SETFL, FNBLOCK);
#elif defined(WINSOCK)
  (void)fd;
  (void)which;
  u_long mode = which ? 1 : 0;
  return ioctlsocket (fd, FIONBIO, &mode);
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
