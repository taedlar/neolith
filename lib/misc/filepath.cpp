#include "filepath.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
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

int path_is_legal_relative(const char *path) {
  if (!path) {
    return 0;
  }

  std::string input(path);
  if (input.find('#') != std::string::npos) {
    return 0;
  }

#ifdef _WIN32
  if (input.find(':') != std::string::npos) {
    return 0;
  }
#endif

  fs::path parsed(input);
  if (parsed.has_root_name() || parsed.has_root_directory() || parsed.is_absolute()) {
    return 0;
  }

  if (has_dot_components(parsed)) {
    return 0;
  }

  return 1;
}

int path_is_within_root(const char *path, const char *root) {
  if (!path || !root || path[0] == '\0' || root[0] == '\0') {
    return 0;
  }

  fs::path input_path(path);
  fs::path root_path(root);

  if (!input_path.is_absolute() || !root_path.is_absolute()) {
    return 0;
  }

  bool ok = false;
  fs::path normalized_root = normalize_existing_or_lexical(root_path, &ok);
  if (!ok) {
    return 0;
  }

  fs::path normalized_input = normalize_existing_or_lexical(input_path, &ok);
  if (!ok) {
    return 0;
  }

  auto root_it = normalized_root.begin();
  auto path_it = normalized_input.begin();

  for (; root_it != normalized_root.end(); ++root_it, ++path_it) {
    if (path_it == normalized_input.end()) {
      return 0;
    }
    if (!component_equal(*root_it, *path_it)) {
      return 0;
    }
  }

  return 1;
}
