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

TEST_F(LPCInterpreterTest, callFunction) {
    // compile a simple test file
    program_t* prog = compile_file(-1, "simple.c",
        "int add(int a, int b) { return a + b; }\n"
    );
    ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";
    EXPECT_EQ(prog->num_functions_defined, 1) << "Expected 1 defined function.";

    // no object is created; we just call the functions directly
    // (no global variables used in the test functions)
    int index, fio, vio;
    svalue_t ret;
    program_t* found_prog = find_function(prog, findstring("add"), &index, &fio, &vio);
    ASSERT_EQ(found_prog, prog) << "find_function did not return the expected program for add().";

    push_number(1);
    push_number(2);
    call_function (prog, index, 2, &ret);

    EXPECT_EQ(ret.type, T_NUMBER) << "Expected return type to be T_NUMBER.";
    EXPECT_EQ(ret.u.number, 3) << "Expected return value of add(1,2) to be 3.";
    free_prog(prog, 1);
}

TEST_F(LPCInterpreterTest, evalCostLimit) {
    // compile a simple test file
    program_t* prog = compile_file(-1, "huge_loop.c",
        "void create() { int j; j = 0; while (j < 100000) { j = j + 1; } }\n"
    );
    ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";
    EXPECT_EQ(prog->num_functions_defined, 1) << "Expected 1 defined function.";

    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
        debug_message("***** expected error: eval_cost too big.");
        pop_context (&econ);
        free_prog(prog, 1);
        return;
    }
    else {
        // no object is created; we just call the functions directly
        // (no global variables used in the test functions)
        int index, fio, vio;
        program_t* found_prog = find_function(prog, findstring("create"), &index, &fio, &vio);
        ASSERT_EQ(found_prog, prog) << "find_function did not return the expected program for create().";

        // set a low eval cost limit
        eval_cost = 500; // should be enough to run out of eval cost in the loop
        call_function (prog, index, 0, 0);
    }
    pop_context (&econ);
    free_prog(prog, 1);
    FAIL() << "Expected too long evaluation error was not raised.";
}
