#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "misc/filepath.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <limits.h>
#include <string>

namespace fs = std::filesystem;

TEST(FilepathHelpersTest, IsPathDescendantAcceptsRelativeAndDot) {
  EXPECT_TRUE(is_path_descendant("."));
  EXPECT_TRUE(is_path_descendant("areas/castle/room"));
}

TEST(FilepathHelpersTest, IsPathDescendantRejectsTraversalAndAbsolute) {
  EXPECT_FALSE(is_path_descendant("../etc/passwd"));
  EXPECT_FALSE(is_path_descendant("areas/../secret"));
  EXPECT_FALSE(is_path_descendant("/absolute/path"));
  EXPECT_FALSE(is_path_descendant("areas/room#1"));
}

TEST(FilepathHelpersTest, IsPathWithinRootRequiresAbsolutePaths) {
  EXPECT_FALSE(is_path_within_root("relative/path", "/tmp"));
  EXPECT_FALSE(is_path_within_root("/tmp/path", "relative/root"));
}

TEST(FilepathHelpersTest, IsPathWithinRootAcceptsDescendantAndRejectsEscape) {
  fs::path root = fs::temp_directory_path() / "neolith_filepath_root";
  fs::path child = root / "a" / "b";
  fs::path outside = fs::temp_directory_path() / "neolith_filepath_outside";

  EXPECT_TRUE(is_path_within_root(child.string().c_str(), root.string().c_str()));
  EXPECT_FALSE(is_path_within_root(outside.string().c_str(), root.string().c_str()));
}

TEST(FilepathHelpersTest, FilepathStripRemovesTrailingSeparatorsInPlace) {
  char path[] = "/var/log/neolith///";
  filepath_strip(path);
  EXPECT_STREQ(path, "/var/log/neolith");

  char root[] = "/";
  filepath_strip(root);
  EXPECT_STREQ(root, "/");
}

TEST(FilepathHelpersTest, FilepathJoinCombinesAndBoundsChecks) {
  char out[PATH_MAX];
  ASSERT_TRUE(filepath_join("/tmp/neolith", "log/debug.log", out, sizeof(out)));
  EXPECT_STREQ(out, "/tmp/neolith/log/debug.log");

  char tiny[8];
  EXPECT_FALSE(filepath_join("/tmp/neolith", "log/debug.log", tiny, sizeof(tiny)));
}

TEST(FilepathHelpersTest, ResolveWithOriginHandlesRelativeAndAbsoluteInput) {
  fs::path temp_dir = fs::temp_directory_path() / "neolith_filepath_origin";
  fs::path config_path = temp_dir / "conf" / "neolith.conf";
  fs::path absolute_input = temp_dir / "shared" / "logs";

  std::error_code ec;
  fs::create_directories(config_path.parent_path(), ec);
  ASSERT_FALSE(ec) << ec.message();
  fs::create_directories(absolute_input.parent_path(), ec);
  ASSERT_FALSE(ec) << ec.message();

  char out[PATH_MAX];
  ASSERT_TRUE(filepath_resolve_with_origin("logs", config_path.string().c_str(), out, sizeof(out)));
  fs::path expected_relative = fs::weakly_canonical(config_path.parent_path() / "logs");
  EXPECT_EQ(fs::path(out).generic_string(), expected_relative.generic_string());

  ASSERT_TRUE(filepath_resolve_with_origin(absolute_input.string().c_str(), config_path.string().c_str(), out, sizeof(out)));
  fs::path expected_absolute = fs::weakly_canonical(absolute_input);
  EXPECT_EQ(fs::path(out).generic_string(), expected_absolute.generic_string());
}

TEST(FilepathHelpersTest, ResolveWithOriginUsesDirectoryOriginDirectly) {
  fs::path temp_dir = fs::temp_directory_path() / "neolith_filepath_origin_dir";
  fs::path origin_dir = temp_dir / "conf";

  std::error_code ec;
  fs::create_directories(origin_dir, ec);
  ASSERT_FALSE(ec) << ec.message();

  char out[PATH_MAX];
  ASSERT_TRUE(filepath_resolve_with_origin("logs", origin_dir.string().c_str(), out, sizeof(out)));

  fs::path expected = fs::weakly_canonical(origin_dir / "logs");
  EXPECT_EQ(fs::path(out).generic_string(), expected.generic_string());
}
