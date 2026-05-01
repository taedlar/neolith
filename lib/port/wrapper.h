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

#ifndef HAVE_UNISTD_H
int symlink (const char* target, const char* linkpath);
#endif

char *strput(char *dest, char *end, const char *src);
char *strput_int(char *, char *, int);

#ifndef HAVE_REALPATH
char* realpath(const char* path, char* resolved_path);
#endif

#ifdef __cplusplus
}
#endif
