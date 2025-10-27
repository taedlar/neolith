#pragma once

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

char* xstrdup(const char* s);
void* xcalloc(size_t nmemb, size_t size);

#ifndef	HAVE_STPCPY
char* stpcpy (char* dest, const char* src);     /* POSIX-1.2008, <string.h> */
#endif	/* ! HAVE_STPCPY */

#ifndef	HAVE_STPNCPY
char* stpncpy (char* dest, const char* src, size_t n);     /* POSIX-1.2008, <string.h> */
#endif	/* ! HAVE_STPNCPY */

#ifdef __cplusplus
}
#endif
