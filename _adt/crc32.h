/*  $Id: crc32.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifndef CRC32_H
#define CRC32_H

#ifdef	HAVE_STDINT_H
  /* ISO C99 header, get definition of uint32_t */
  #include <stdint.h>
#elif	HAVE_SYS_INTTYPES_H
  /* BSD defines integer types here */
  #include <sys/inttypes.h>
#else
  typedef unsigned int	uint32_t
#endif

uint32_t compute_crc32(unsigned char *, int);

#endif	/* ! CRC32_H */
