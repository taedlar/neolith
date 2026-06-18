#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"
#include "lpc/program/binaries.h"

#include <system_error>
#include <cstring>

using namespace testing;

#ifdef BINARIES
namespace {
bool programHasString(program_t* prog, const char* bytes, size_t length) {
    for (int i = 0; i < static_cast<int>(prog->num_strings); i++) {
        if (SHARED_STRLEN(prog->strings[i]) != length)
            continue;
        if (memcmp(prog->strings[i], bytes, length) == 0)
            return true;
    }
    return false;
}
}

TEST_F(LPCCompilerTest, compileWithSaveBinary) {
    ASSERT_NE(CONFIG_STR(__SAVE_BINARIES_DIR__), nullptr)
        << "__SAVE_BINARIES_DIR__ is not configured.";
    namespace fs = std::filesystem;

    const fs::path mudlib_root = fs::path(MAIN_OPTION(mudlib_dir_absolute));
    ASSERT_TRUE(fs::exists(mudlib_root)) << "Mudlib root does not exist: " << mudlib_root;
    ASSERT_LT(mudlib_root.string().size(), static_cast<size_t>(PATH_MAX));
    strncpy(MAIN_OPTION(mudlib_dir_absolute), mudlib_root.string().c_str(), PATH_MAX - 1);
    MAIN_OPTION(mudlib_dir_absolute)[PATH_MAX - 1] = '\0';

    init_binaries();

    // ensure no leftover binary from previous runs
    std::error_code ec;
    fs::remove (fs::path(MAIN_OPTION(mudlib_dir_absolute)) / "bin/api/unicode.b", ec);

    program_t* prog = load_binary("api/unicode.c", 0); // the source file does not contain #pragma save_binary
    EXPECT_TRUE(prog == nullptr) << "Binary should not exist before compilation.";

    /*  The #pragma save_binary is checked in epilog(), which is called
     *  as the last step in compile_file(). A master apply "valid_save_binary"
     *  is called to confirm saving is allowed.
     *
     *  Saving binary should not rely on CWD being set to the mudlib directory, but it
     *  does require the file path to be correct.
     */
    fs::path source = fs::path(MAIN_OPTION(mudlib_dir_absolute)) / "api/unicode.c";
    int fd = FILE_OPEN(source.u8string().c_str(), O_RDONLY);
    ASSERT_NE(fd, -1) << "Failed to open api/unicode.c for reading.";
    prog = compile_file(fd, "api/unicode.c", "#pragma save_binary"); // now with #pragma save_binary
    EXPECT_TRUE(prog != nullptr) << "compile_file returned null program.";
    total_lines = 0;
    FILE_CLOSE(fd);

    // free the compiled program
    free_prog(prog, 1);

    /* Load the saved binary:
     * Three conditions must be met to load the binary:
     * 1. The binary file must exist in __SAVE_BINARIES_DIR__
     * 2. The binary's driver_id and config_id must match the current ones
     * 3. The source file, included files and all inherited files must be older than the binary
     */
    prog = load_binary("api/unicode.c", 0);
    ASSERT_NE(prog, nullptr) << "load_binary failed to load saved binary.";
    free_prog(prog, 1);
}

TEST_F(LPCCompilerTest, saveBinaryPreservesEmbeddedNullStringLiteral) {
    ASSERT_NE(CONFIG_STR(__SAVE_BINARIES_DIR__), nullptr)
        << "__SAVE_BINARIES_DIR__ is not configured.";
    init_binaries();
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::remove (fs::path(MAIN_OPTION(mudlib_dir_absolute)) / "bin/tests/test_save_binary/bytespan.b", ec); //

    program_t* prog = compile_file(-1, "tests/test_save_binary/bytespan.c", R"(
        #pragma save_binary

        string embedded_null_octal() {
          return "ab\0cd";
        }

        string embedded_null_hex() {
          return "ab\x00yz";
        }
    )");
    EXPECT_TRUE(prog != nullptr) << "compile_file returned null program.";
    total_lines = 0;

    ASSERT_TRUE(programHasString(prog, "ab\0cd", 5))
        << "Compiled program string table did not retain embedded-null literal bytes.";
    ASSERT_TRUE(programHasString(prog, "ab\0yz", 5))
        << "Compiled program string table did not retain embedded-null hex literal bytes.";

    free_prog(prog, 1);

    prog = load_binary("tests/test_save_binary/bytespan.c", BIN_IGNORE_SOURCE_FILE); // source is pre_text, so ignore source file check
    ASSERT_TRUE(prog != nullptr) << "load_binary failed to load saved binary for tests/test_save_binary/bytespan.c.";

    EXPECT_TRUE(programHasString(prog, "ab\0cd", 5))
        << "Loaded binary string table lost embedded-null bytes for octal literal.";
    EXPECT_TRUE(programHasString(prog, "ab\0yz", 5))
        << "Loaded binary string table lost embedded-null bytes for hex literal.";

    free_prog(prog, 1);
}

TEST_F(LPCCompilerTest, loadBinaryUsesVerifiedMudlibPathOutsideMudlibCwd) {
    ASSERT_NE(CONFIG_STR(__SAVE_BINARIES_DIR__), nullptr)
        << "__SAVE_BINARIES_DIR__ is not configured.";

    namespace fs = std::filesystem;
    const fs::path mudlib_cwd = fs::current_path();
    const fs::path shifted_cwd = mudlib_cwd.parent_path();

    ASSERT_LT(mudlib_cwd.string().size(), static_cast<size_t>(PATH_MAX));
    strncpy(MAIN_OPTION(mudlib_dir_absolute), mudlib_cwd.string().c_str(), PATH_MAX - 1);
    MAIN_OPTION(mudlib_dir_absolute)[PATH_MAX - 1] = '\0';
    SET_CONFIG_STR(__SIMUL_EFUN_FILE__, "config.h");
    init_binaries();

    std::error_code ec;
    fs::remove(fs::path(MAIN_OPTION(mudlib_dir_absolute)) / "bin/tests/test_save_binary/pathcheck.b", ec);

    program_t* prog = compile_file(-1, "tests/test_save_binary/pathcheck.c", R"(
        #pragma save_binary
        int run_test() { return 42; }
    )");
    ASSERT_NE(prog, nullptr) << "compile_file returned null program.";
    total_lines = 0;
    free_prog(prog, 1);

    fs::current_path(shifted_cwd);
    prog = load_binary("tests/test_save_binary/pathcheck.c", BIN_IGNORE_SOURCE_FILE | BIN_IGNORE_INCLUDE_FILES);
    ASSERT_NE(prog, nullptr) << "load_binary failed outside mudlib cwd.";
    free_prog(prog, 1);

    fs::current_path(mudlib_cwd);
}
#endif
