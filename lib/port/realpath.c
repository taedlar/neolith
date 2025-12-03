#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <limits.h>

#ifdef _WIN32
#ifndef HAVE_REALPATH
char* realpath(const char* path, char* resolved_path)
{
    /* On Windows, _fullpath can be used to achieve similar functionality */
    return _fullpath(resolved_path, path, _MAX_PATH);
}
#endif /* !HAVE_REALPATH */
#endif  /* _WIN32 */
