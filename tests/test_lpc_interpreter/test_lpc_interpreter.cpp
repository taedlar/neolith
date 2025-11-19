#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fixtures.hpp"

extern "C" {
    #include "lpc/program.h"
    #include "lpc/program/disassemble.h"
}

TEST_F(LPCInterpreterTest, disassemble) {
    // compile a simple test file
    program_t* prog = compile_file(-1, "master.c",
        "int i; // global\n"
        "void create() { i = 1234; }\n"
    );
    ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";
    total_lines = 0;

    EXPECT_EQ(prog->num_functions_defined, 1) << "Expected 1 defined function.";
    EXPECT_EQ(prog->num_variables_total, 1) << "Expected 1 global variable.";
    
    compiler_function_t* funp = prog->function_table; // index 0
    EXPECT_STREQ(funp->name, "create") << "First function is not create().";

    EXPECT_NO_THROW(disassemble (stderr, prog->program, 0, prog->program_size, prog));

    // free the compiled program
    free_prog(prog, 1);
}
