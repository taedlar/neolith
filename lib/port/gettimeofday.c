#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <inttypes.h>

/* Winsock2 includes struct timeval definition to support select(), let's borrow from it */
#include <WinSock2.h>

#ifndef HAVE_GETTIMEOFDAY
int gettimeofday (struct timeval* tp, void* tzp) {
   FILETIME file_time;
   ULARGE_INTEGER ularge;
   const uint64_t EPOCH = 116444736000000000ULL; // Epoch offset in 100-nanosecond intervals
   GetSystemTimeAsFileTime(&file_time);
   ularge.LowPart = file_time.dwLowDateTime;
   ularge.HighPart = file_time.dwHighDateTime;
   uint64_t time = (ularge.QuadPart - EPOCH) / 10; // Convert to microseconds
   tp->tv_sec = (long)(time / 1000000);
   tp->tv_usec = (long)(time % 1000000);
   return 0;
}
#endif /* !HAVE_GETTIMEOFDAY */

#endif /* _WIN32 */
