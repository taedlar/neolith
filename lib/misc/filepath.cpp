#include "filepath.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <limits.h>
#include <string>

namespace fs = std::filesystem;

namespace {

bool component_equal(const fs::path &lhs, const fs::path &rhs) {
#ifdef _WIN32
  std::string ls = lhs.generic_string();
  std::string rs = rhs.generic_string();
  if (ls.size() != rs.size()) {
    return false;
  }
  for (size_t i = 0; i < ls.size(); i++) {
    if (std::tolower(static_cast<unsigned char>(ls[i])) !=
        std::tolower(static_cast<unsigned char>(rs[i]))) {
      return false;
    }
  }
  return true;
#else
  return lhs == rhs;
#endif
}

bool has_dot_components(const fs::path &path) {
  for (const auto &part : path) {
    if (part == "." || part == "..") {
      return true;
    }
  }
  return false;
}

fs::path normalize_existing_or_lexical(const fs::path &path, bool *ok) {
  std::error_code ec;
  fs::path canonical = fs::weakly_canonical(path, ec);
  if (!ec) {
    *ok = true;
    return canonical;
  }

  fs::path lexical = path.lexically_normal();
  if (!lexical.is_absolute()) {
    *ok = false;
    return {};
  }

  *ok = true;
  return lexical;
}

}  // namespace

bool path_is_legal_relative(const char *path) {
  if (!path) {
    return false;
  }

  std::string input(path);
  if (input.find('#') != std::string::npos) {
    return false;
  }

#ifdef _WIN32
  if (input.find(':') != std::string::npos) {
    return false;
  }
#endif

  fs::path parsed(input);
  if (parsed.has_root_name() || parsed.has_root_directory() || parsed.is_absolute()) {
    return false;
  }

  if (has_dot_components(parsed)) {
    return false;
  }

  return true;
}

bool path_is_within_root(const char *path, const char *root) {
  if (!path || !root || path[0] == '\0' || root[0] == '\0') {
    return false;
  }

  fs::path input_path(path);
  fs::path root_path(root);

  if (!input_path.is_absolute() || !root_path.is_absolute()) {
    return false;
  }

  bool ok = false;
  fs::path normalized_root = normalize_existing_or_lexical(root_path, &ok);
  if (!ok) {
    return false;
  }

  fs::path normalized_input = normalize_existing_or_lexical(input_path, &ok);
  if (!ok) {
    return false;
  }

  auto root_it = normalized_root.begin();
  auto path_it = normalized_input.begin();

  for (; root_it != normalized_root.end(); ++root_it, ++path_it) {
    if (path_it == normalized_input.end()) {
      return false;
    }
    if (!component_equal(*root_it, *path_it)) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Resolve a relative path to an absolute path within mudlib and verify containment.
 *
 * Takes a relative path (e.g., from check_valid_path) and mudlib root directory,
 * combines them into an absolute path, and verifies the result is within the mudlib root
 * to prevent directory traversal attacks.
 *
 * Uses C++17 std::filesystem for robust path normalization and component comparison.
 *
 * @param relative_path The mudlib-local path (e.g., "." or "obj/player")
 * @param mudlib_root   The absolute mudlib root directory
 * @return malloc'd absolute path if valid and within root, nullptr on error.
 *         Returns exactly the mudlib_root if relative_path is "."
 *         Caller must free the returned string with free().
 */
extern "C" {
char *resolve_path_in_mudlib(const char *relative_path, const char *mudlib_root) {
  if (!relative_path || !mudlib_root || mudlib_root[0] == '\0') {
    return nullptr;
  }

  // Handle the special case where "." means the mudlib root itself
  if (relative_path[0] == '.' && relative_path[1] == '\0') {
    return strdup(mudlib_root);
  }

  // Construct absolute path: mudlib_root / relative_path
  fs::path root(mudlib_root);
  std::string combined = root.generic_string();
  combined += '/';
  combined += relative_path;

  // Check bounds before attempting allocation
  if (combined.length() + 1 > PATH_MAX) {
    return nullptr;
  }

  // Verify the combined path is within the mudlib root
  if (!path_is_within_root(combined.c_str(), mudlib_root)) {
    return nullptr;
  }

  // Return malloc'd copy of the combined path
  return strdup(combined.c_str());
}
}  // extern "C"
