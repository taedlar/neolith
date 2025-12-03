#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef _WIN32
int symlink(const char *target, const char *linkpath)
{
  /* Windows Vista and later support symlinks natively, but creating them
   * requires either admin privileges or developer mode enabled.
   * The security considerations and API complexity make it impractical to implement
   * a full-featured symlink function here.
   */
  return -1; /* Indicate failure */
}
#endif
