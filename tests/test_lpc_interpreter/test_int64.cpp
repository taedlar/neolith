#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fixtures.hpp"

extern "C" {
    #include "lpc/program.h"
    #include "lpc/program/disassemble.h"
}

TEST_F(LPCInterpreterTest, int64_SmallValues) {
    // Test that small values still work correctly with int64_t runtime
    program_t* prog = compile_file(-1, "int64_small.c",
        "int test() { return 42; }\n"
    );
    ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";
    
    int index, fio, vio;
    svalue_t ret;
    program_t* found_prog = find_function(prog, findstring("test"), &index, &fio, &vio);
    ASSERT_EQ(found_prog, prog) << "find_function did not return expected program.";
    int runtime_index = found_prog->function_table[index].runtime_index;

    call_function (prog, runtime_index, 0, &ret);

    EXPECT_EQ(ret.type, T_NUMBER) << "Expected return type to be T_NUMBER.";
    EXPECT_EQ(ret.u.number, 42) << "Expected return value of test() to be 42.";
    free_prog(prog, 1);
}

TEST_F(LPCInterpreterTest, int64_LargePositive) {
    // Test large positive value that exceeds 32-bit range
    program_t* prog = compile_file(-1, "int64_large.c",
        "int test() { return 2147483648; }\n"  // INT32_MAX + 1
    );
    ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";
    
    int index, fio, vio;
    svalue_t ret;
    program_t* found_prog = find_function(prog, findstring("test"), &index, &fio, &vio);
    ASSERT_EQ(found_prog, prog) << "find_function did not return expected program.";
    int runtime_index = found_prog->function_table[index].runtime_index;

    call_function (prog, runtime_index, 0, &ret);

    EXPECT_EQ(ret.type, T_NUMBER) << "Expected return type to be T_NUMBER.";
    EXPECT_EQ(ret.u.number, 2147483648LL) << "Expected return value to be 2147483648 (INT32_MAX + 1).";
    free_prog(prog, 1);
}

TEST_F(LPCInterpreterTest, int64_LargeNegative) {
    // Test large negative value that exceeds 32-bit range
    program_t* prog = compile_file(-1, "int64_large_neg.c",
        "int test() { return -2147483649; }\n"  // INT32_MIN - 1
    );
    ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";
    
    int index, fio, vio;
    svalue_t ret;
    program_t* found_prog = find_function(prog, findstring("test"), &index, &fio, &vio);
    ASSERT_EQ(found_prog, prog) << "find_function did not return expected program.";
    int runtime_index = found_prog->function_table[index].runtime_index;

    call_function (prog, runtime_index, 0, &ret);

    EXPECT_EQ(ret.type, T_NUMBER) << "Expected return type to be T_NUMBER.";
    EXPECT_EQ(ret.u.number, -2147483649LL) << "Expected return value to be -2147483649 (INT32_MIN - 1).";
    free_prog(prog, 1);
}

TEST_F(LPCInterpreterTest, int64_Arithmetic) {
    // Test arithmetic operations with 64-bit integers
    program_t* prog = compile_file(-1, "int64_arithmetic.c",
        "int test_add() { return 2147483647 + 2; }\n"
        "int test_mult() { return 1000000000 * 3; }\n"
    );
    ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";
    
    // Test addition
    int index, fio, vio;
    svalue_t ret;
    program_t* found_prog = find_function(prog, findstring("test_add"), &index, &fio, &vio);
    ASSERT_EQ(found_prog, prog) << "find_function did not return expected program.";
    int runtime_index = found_prog->function_table[index].runtime_index;

    call_function (prog, runtime_index, 0, &ret);

    EXPECT_EQ(ret.type, T_NUMBER) << "Expected return type to be T_NUMBER.";
    EXPECT_EQ(ret.u.number, 2147483649LL) << "Expected 2147483647 + 2 = 2147483649.";
    
    // Test multiplication
    found_prog = find_function(prog, findstring("test_mult"), &index, &fio, &vio);
    ASSERT_EQ(found_prog, prog) << "find_function did not return expected program.";
    runtime_index = found_prog->function_table[index].runtime_index;

    call_function (prog, runtime_index, 0, &ret);

    EXPECT_EQ(ret.type, T_NUMBER) << "Expected return type to be T_NUMBER.";
    EXPECT_EQ(ret.u.number, 3000000000LL) << "Expected 1000000000 * 3 = 3000000000.";
    
    free_prog(prog, 1);
}

TEST_F(LPCInterpreterTest, int64_MaxValue) {
    // Test near INT64_MAX value
    program_t* prog = compile_file(-1, "int64_max.c",
        "int test() { return 9223372036854775806; }\n"  // INT64_MAX - 1
    );
    ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";
    
    int index, fio, vio;
    svalue_t ret;
    program_t* found_prog = find_function(prog, findstring("test"), &index, &fio, &vio);
    ASSERT_EQ(found_prog, prog) << "find_function did not return expected program.";
    int runtime_index = found_prog->function_table[index].runtime_index;

    call_function (prog, runtime_index, 0, &ret);

    EXPECT_EQ(ret.type, T_NUMBER) << "Expected return type to be T_NUMBER.";
    EXPECT_EQ(ret.u.number, 9223372036854775806LL) << "Expected return value to be INT64_MAX - 1.";
    free_prog(prog, 1);
}
