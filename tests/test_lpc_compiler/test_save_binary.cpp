#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <system_error>
#include <cstring>

#include "fixtures.hpp"
#include "lpc/program/binaries.h"

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

TEST_F(LPCCompilerTest, saveBinary) {
    ASSERT_NE(CONFIG_STR(__SAVE_BINARIES_DIR__), nullptr)
        << "__SAVE_BINARIES_DIR__ is not configured.";
    init_binaries();

    /*  The #pragma save_binary is checked in epilog(), which is called
        *  as the last step in compile_file(). A master apply "valid_save_binary"
        *  is called to confirm saving is allowed.
        */
    int fd = FILE_OPEN("api/unicode.c", O_RDONLY);
    ASSERT_NE(fd, -1) << "Failed to open api/unicode.c for reading.";
    program_t* prog = compile_file(fd, "api/unicode.c", nullptr); // save_binary pragma is enabled in this file
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
    prog = load_binary("api/unicode.c");
    EXPECT_TRUE(prog != nullptr) << "load_binary failed to load saved binary.";
    free_prog(prog, 1);
}

TEST_F(LPCCompilerTest, saveBinaryPreservesEmbeddedNullStringLiteral) {
    ASSERT_NE(CONFIG_STR(__SAVE_BINARIES_DIR__), nullptr)
        << "__SAVE_BINARIES_DIR__ is not configured.";
    init_binaries();

    int fd = FILE_OPEN("api/bytespan.c", O_RDONLY);
    ASSERT_NE(fd, -1) << "Failed to open api/bytespan.c for reading.";
    program_t* prog = compile_file(fd, "api/bytespan.c", nullptr);
    EXPECT_TRUE(prog != nullptr) << "compile_file returned null program.";
    total_lines = 0;
    FILE_CLOSE(fd);

    ASSERT_TRUE(programHasString(prog, "ab\0cd", 5))
        << "Compiled program string table did not retain embedded-null literal bytes.";
    ASSERT_TRUE(programHasString(prog, "ab\0yz", 5))
        << "Compiled program string table did not retain embedded-null hex literal bytes.";

    free_prog(prog, 1);

    prog = load_binary("api/bytespan.c");
    ASSERT_TRUE(prog != nullptr) << "load_binary failed to load saved binary for api/bytespan.c.";

    EXPECT_TRUE(programHasString(prog, "ab\0cd", 5))
        << "Loaded binary string table lost embedded-null bytes for octal literal.";
    EXPECT_TRUE(programHasString(prog, "ab\0yz", 5))
        << "Loaded binary string table lost embedded-null bytes for hex literal.";

    free_prog(prog, 1);
}
#endif
