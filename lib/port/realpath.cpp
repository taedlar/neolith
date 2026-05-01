#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <limits.h>

#ifndef HAVE_REALPATH
extern "C"
char* realpath(const char* path, char* resolved_path)
{
    /* On Windows, _fullpath can be used to achieve similar functionality */
    return _fullpath(resolved_path, path, _MAX_PATH);
}
#endif /* !HAVE_REALPATH */
