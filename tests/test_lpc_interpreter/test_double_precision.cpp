#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fixtures.hpp"

#include "lpc/program.h"

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
    lpc::svalue ret;
    program_t* found_prog = find_function(prog, findstring("bar", NULL), &index, &fio, &vio);
    ASSERT_EQ(found_prog, prog) << "find_function did not return the expected program for bar().";
    current_prog = prog; // set current_prog for the calling local function
    int runtime_index = found_prog->function_table[index].runtime_index;
    call_function (prog, runtime_index, 0, ret.raw());

    auto ret_view = ret.view();
    EXPECT_TRUE(ret_view.is_real()) << "Expected return type to be T_REAL.";
    EXPECT_DOUBLE_EQ(ret_view.real(), 6.28) << "Expected return value of bar() to be 6.28 (no suffix f).";
    free_prog(prog, 1);
}
