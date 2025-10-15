#pragma once

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
