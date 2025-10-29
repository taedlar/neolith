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
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
        // FAIL() << "Failed to save binary for test object.";
    }
    else {
        init_binaries();
        std::error_code ec;
        if (std::filesystem::exists("bin/save_binary.b", ec)) {
            std::filesystem::remove("bin/save_binary.b", ec);
        }

        /*  The #pragma save_binary is checked in epilog(), which is called
         *  as the last step in compile_file(). A master apply "valid_save_binary"
         *  is called to confirm saving is allowed.
         */
        int fd = open("save_binary.c", O_RDONLY);
        ASSERT_NE(fd, -1) << "Failed to open save_binary.c for reading.";
        program_t* prog = compile_file(fd, "save_binary.c");
        ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";
        total_lines = 0;
        close(fd);

        // free the compiled program
        free_prog(prog, 1);

        ASSERT_TRUE(std::filesystem::exists("bin/save_binary.b", ec))
            << "Binary file was not created by save_binary pragma.";

        init_simulate();
        object_t* obj = load_object("/save_binary.c");
        current_object = obj;
        destruct_object(obj);
        tear_down_simulate();
    }
    pop_context (&econ);
}
#endif
