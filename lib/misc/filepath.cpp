#include "filepath.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <limits.h>
#include <string>

namespace fs = std::filesystem;

static bool component_equal(const fs::path &lhs, const fs::path &rhs) {
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

static fs::path normalize_existing_or_lexical(const fs::path &path, bool *ok) {
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

bool is_path_descendant(const char *path) {
  if (!path)
    return false;

  std::string input(path);
  if (input == ".")
    return true;

  if (input.find('#') != std::string::npos)
    return false; // Disallow '#' to prevent confusion with object file paths

#ifdef _WIN32
  if (input.find(':') != std::string::npos)
    return false; // Disallow ':' to prevent drive letters and alternate data streams
#endif

  fs::path parsed(input);
  if (parsed.has_root_name() || parsed.has_root_directory() || parsed.is_absolute())
    return false;

  for (const auto &part : parsed) {
    if (part == "." || part == "..")
      return false;
  }

  return true;
}

bool is_path_within_root(const char *path, const char *root) {
  if (!path || !root || path[0] == '\0' || root[0] == '\0')
    return false;

  fs::path input_path(path);
  fs::path root_path(root);

  if (!input_path.is_absolute() || !root_path.is_absolute())
    return false;

  bool ok = false;
  fs::path normalized_root = normalize_existing_or_lexical(root_path, &ok);
  if (!ok)
    return false;

  fs::path normalized_input = normalize_existing_or_lexical(input_path, &ok);
  if (!ok)
    return false;

  auto root_it = normalized_root.begin();
  auto path_it = normalized_input.begin();

  for (; root_it != normalized_root.end(); ++root_it, ++path_it) {
    if (path_it == normalized_input.end())
      return false;
    if (!component_equal(*root_it, *path_it))
      return false;
  }

  return true;
}

bool filepath_strip_trailing_separators(const char *path, char *out, size_t out_size) {
  if (!path || !out || out_size == 0) {
    return false;
  }

  try {
    std::string normalized = fs::path(path).generic_string();
    if (normalized.empty()) {
      return false;
    }

    while (normalized.size() > 1 && normalized.back() == '/') {
      normalized.pop_back();
    }

    if (normalized.size() >= out_size) {
      return false;
    }

    std::memcpy(out, normalized.c_str(), normalized.size() + 1);
    return true;
  } catch (...) {
    return false;
  }
}

bool filepath_join_dir_and_basename(const char *dir, const char *path, char *out, size_t out_size) {
  if (!dir || !path || !out || out_size == 0) {
    return false;
  }

  try {
    fs::path base = fs::path(path).filename();
    if (base.empty()) {
      return false;
    }

    std::string joined = (fs::path(dir) / base).generic_string();
    if (joined.size() >= out_size) {
      return false;
    }

    std::memcpy(out, joined.c_str(), joined.size() + 1);
    return true;
  } catch (...) {
    return false;
  }
}

bool filepath_resolve_mudlib_dir(const char *mudlib_dir, const char *config_file,
                                 char *out, size_t out_size) {
  if (!mudlib_dir || !config_file || !out || out_size == 0)
    return false;

  try {
    fs::path mud_path(mudlib_dir);
    fs::path base;

    if (mud_path.is_absolute())
      {
        base = mud_path;
      }
    else
      {
        fs::path config_dir = fs::path(config_file).parent_path();
        if (config_dir.empty())
          config_dir = ".";
        base = config_dir / mud_path;
      }

    std::error_code ec;
    fs::path canonical = fs::weakly_canonical(base, ec);
    if (ec)
      return false;

    std::string result = canonical.string();
    if (result.size() >= out_size)
      return false;

    std::memcpy(out, result.c_str(), result.size() + 1);
    return true;
  } catch (...) {
    return false;
  }
}
