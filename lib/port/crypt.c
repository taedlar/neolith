#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

/* crypt(3) is a POSIX C library function (libcrypt) typically used
 * for computing password hashes.
 * On Windows, this function is not available by default, so we
 * provide a stub implementation here.
 * 
 * Reference: https://en.wikipedia.org/wiki/Crypt_(C)
 */

 #ifdef _WIN32
 char* crypt(const char* key, const char* salt) {
     /* TODO: implementation for Windows */
     return "xx";
 }
 #endif /* _WIN32 */
