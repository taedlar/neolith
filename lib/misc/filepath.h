#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_STDBOOL_H
  #include <stdbool.h>
#elif !defined(__cplusplus)
  typedef int bool;
  #define true 1
  #define false 0
#endif

bool path_is_legal_relative(const char *path);
bool path_is_within_root(const char *path, const char *root);
char *resolve_path_in_mudlib(const char *relative_path, const char *mudlib_root);

#ifdef __cplusplus
}
#endif
