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

/**
 * @brief Checks if the given path only contains valid characters and does not
 *        attempt to traverse outside where it starts.
 *
 * This is a C++17 implementation of the legacy legal_path() function, using
 * std::filesystem for robust path parsing and validation.
 *
 * When passing a path to LPC code, it must pass this validation to ensure
 * consistent view of the filesystem. This includes:
 * - Header includes via #include
 * - LPC object loading (include inherits)
 * - Master applies for file access permissions
 * - File I/O efuns (e.g., read_file, write_file)
 * - Any other code that takes file paths from LPC and needs to ensure they are
 *   within the mudlib directory structure.
 *
 * If the path is valid, it can then be resolved to an absolute host file path using
 * resolve_path_in_mudlib(), which will also verify that the resolved path is within
 * the mudlib root to prevent directory traversal attacks.
 *
 * @param path The path to validate. It must be normalized to a relative path from
 *             the mudlib root (e.g., "obj/player.c" or ".").
 * @return true if the path is within the mudlib, false otherwise.
 */
bool is_path_descendant(const char *path);

/**
 * @brief Checks if the given absolute path is within the specified root directory.
 *
 * This function ensures that the path is within the root directory, enforcing
 * filesystem sandboxing.
 *
 * @param path The absolute path to check.
 * @param root The root directory to check against (usually the mudlib root).
 * @return true if the path is within the root, false otherwise.
 */
bool is_path_within_root(const char *path, const char *root);

#ifdef __cplusplus
}
#endif
