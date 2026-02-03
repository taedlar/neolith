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
    int runtime_index = found_prog->function_table[index].runtime_index;

    push_number(1);
    push_number(2);
    call_function (prog, runtime_index, 2, &ret);

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
        int runtime_index = found_prog->function_table[index].runtime_index;

        // set a low eval cost limit
        eval_cost = 500; // should be enough to run out of eval cost in the loop
        call_function (prog, runtime_index, 0, 0);
    }
    pop_context (&econ);
    free_prog(prog, 1);
    FAIL() << "Expected too long evaluation error was not raised.";
}

TEST_F(LPCInterpreterTest, foreachUtf8String) {
    // Test foreach loop iterating over UTF-8 characters in a string
    // The loop should iterate over each character correctly, handling multi-byte UTF-8 sequences
    // Note: We use hex escapes for UTF-8 bytes to ensure correct encoding across platforms
    program_t* prog = compile_file(-1, "utf8_foreach.c",
        "int* test_utf8_foreach() {\n"
        "    string s = \"Hello\\xe4\\xb8\\x96\\xe7\\x95\\x8c\";\n"  // "Hello世界" in UTF-8 bytes
        "    int* result = allocate(7);\n"
        "    int i = 0;\n"
        "    foreach(int ch in s) {\n"
        "        result[i++] = ch;\n"
        "    }\n"
        "    return result;\n"
        "}\n"
    );
    ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";
    EXPECT_EQ(prog->num_functions_defined, 1) << "Expected 1 defined function.";

    // Find and call the function
    int index, fio, vio;
    svalue_t ret;
    program_t* found_prog = find_function(prog, findstring("test_utf8_foreach"), &index, &fio, &vio);
    ASSERT_EQ(found_prog, prog) << "find_function did not return the expected program.";
    int runtime_index = found_prog->function_table[index].runtime_index;

    call_function(prog, runtime_index, 0, &ret);

    // Verify the return value is an array
    EXPECT_EQ(ret.type, T_ARRAY) << "Expected return type to be T_ARRAY.";
    ASSERT_TRUE(ret.u.arr != nullptr) << "Expected non-null array.";
    EXPECT_EQ(ret.u.arr->size, 7) << "Expected array size to be 7.";

    // Verify the array contains the correct Unicode code points:
    // 'H' = 72, 'e' = 101, 'l' = 108, 'l' = 108, 'o' = 111
    // '世' = 0x4E16 (19990), '界' = 0x754C (30028)
    EXPECT_EQ(ret.u.arr->item[0].type, T_NUMBER);
    EXPECT_EQ(ret.u.arr->item[0].u.number, 72) << "Expected 'H' (72).";
    EXPECT_EQ(ret.u.arr->item[1].type, T_NUMBER);
    EXPECT_EQ(ret.u.arr->item[1].u.number, 101) << "Expected 'e' (101).";
    EXPECT_EQ(ret.u.arr->item[2].type, T_NUMBER);
    EXPECT_EQ(ret.u.arr->item[2].u.number, 108) << "Expected 'l' (108).";
    EXPECT_EQ(ret.u.arr->item[3].type, T_NUMBER);
    EXPECT_EQ(ret.u.arr->item[3].u.number, 108) << "Expected 'l' (108).";
    EXPECT_EQ(ret.u.arr->item[4].type, T_NUMBER);
    EXPECT_EQ(ret.u.arr->item[4].u.number, 111) << "Expected 'o' (111).";
    EXPECT_EQ(ret.u.arr->item[5].type, T_NUMBER);
    EXPECT_EQ(ret.u.arr->item[5].u.number, 19990) << "Expected '世' (0x4E16 = 19990).";
    EXPECT_EQ(ret.u.arr->item[6].type, T_NUMBER);
    EXPECT_EQ(ret.u.arr->item[6].u.number, 30028) << "Expected '界' (0x754C = 30028).";

    free_svalue(&ret, "test");
    free_prog(prog, 1);
}
