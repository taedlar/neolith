#pragma once

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

char* xstrdup(const char* s);
void* xcalloc(size_t nmemb, size_t size);

#ifndef	HAVE_STPCPY
char* stpcpy (char* dest, const char* src);
#endif	/* ! HAVE_STPCPY */

#ifdef __cplusplus
}
#endif
