#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fixtures.hpp"

extern "C" {
    #include "lpc/program.h"
    #include "lpc/program/disassemble.h"
}

TEST_F(LPCInterpreterTest, disassemble) {
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
        FAIL() << "Failed to compile master.c.";
    }
    else {
        // compile a simple test file
        int fd = open("master.c", O_RDONLY);
        ASSERT_NE(fd, -1) << "Failed to open master.c for reading.";
        program_t* prog = compile_file(fd, "master.c");
        ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";
        total_lines = 0;
        close(fd);

        // compiler_function_t* funp = prog->function_table; // index 0
        // fprintf(stderr, "function#0 name = '%s'\n", funp->name);
        EXPECT_NO_THROW(disassemble (stderr, prog->program, 0, prog->program_size, prog));

        // free the compiled program
        free_prog(prog, 1);
    }
    pop_context (&econ);
}
