#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <system_error>

#include "fixtures.hpp"
extern "C" {
    #include "lpc/program/binaries.h"
}

using namespace testing;

#ifdef BINARIES
TEST_F(LPCCompilerTest, saveBinary) {
    ASSERT_NE(CONFIG_STR(__SAVE_BINARIES_DIR__), nullptr)
        << "__SAVE_BINARIES_DIR__ is not configured.";
    init_binaries();

    /*  The #pragma save_binary is checked in epilog(), which is called
        *  as the last step in compile_file(). A master apply "valid_save_binary"
        *  is called to confirm saving is allowed.
        */
    int fd = FILE_OPEN("save_binary.c", O_RDONLY);
    ASSERT_NE(fd, -1) << "Failed to open save_binary.c for reading.";
    program_t* prog = compile_file(fd, "save_binary.c", nullptr); // save_binary pragma is enabled in this file
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
    prog = load_binary("save_binary.c", UNIT_TESTING);
    EXPECT_TRUE(prog != nullptr) << "load_binary failed to load saved binary.";
    free_prog(prog, 1);
}
#endif
