#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int path_is_legal_relative(const char *path);
int path_is_within_root(const char *path, const char *root);

#ifdef __cplusplus
}
#endif
