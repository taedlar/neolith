#pragma once
extern char* xstrdup(const char* s);
extern void* xcalloc(size_t nmemb, size_t size);

#ifndef	HAVE_STPCPY
extern char* stpcpy (char* dest, const char* src);
#endif	/* ! HAVE_STPCPY */
