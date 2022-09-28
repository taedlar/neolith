/*  $Id: wrapper.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifndef	LIB_WRAPPER_H
#define	LIB_WRAPPER_H

extern char* xstrdup(char* s);
extern void* xcalloc(size_t nmemb, size_t size);

#ifndef	HAVE_STPCPY
extern char* stpcpy (char* dest, const char* src);
#endif	/* ! HAVE_STPCPY */

#endif	/* ! LIB_WRAPPER_H */
