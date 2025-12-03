#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fixtures.hpp"

extern "C" {
    #include "lpc/program.h"
}

TEST_F(LPCInterpreterTest, floatingPointPrecision) {
    // compile a simple test file
    program_t* prog = compile_file(-1, "double.c",
        "double foo() { return 3.14; }\n"
        "double bar() { return foo() * 2.0; }\n"
    );
    ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";
    EXPECT_EQ(prog->num_functions_defined, 2) << "Expected 2 defined function.";

    // no object is created; we just call the functions directly
    // (no global variables used in the test functions)
    int index, fio, vio;
    svalue_t ret;
    program_t* found_prog = find_function(prog, findstring("bar"), &index, &fio, &vio);
    ASSERT_EQ(found_prog, prog) << "find_function did not return the expected program for bar().";
    current_prog = prog; // set current_prog for the calling local function
    call_function (prog, index, 0, &ret);

    EXPECT_EQ(ret.type, T_REAL) << "Expected return type to be T_REAL.";
    EXPECT_DOUBLE_EQ(ret.u.real, 6.28f) << "Expected return value of bar() to be 6.28.";
    free_prog(prog, 1);
}
