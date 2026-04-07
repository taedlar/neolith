#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fixtures.hpp"
#include "lpc/types.hpp"

extern "C" {
    #include "lpc/program.h"
    #include "lpc/program/disassemble.h"
    #include "lpc/buffer.h"
    #include "lpc/mapping.h"
}

namespace {

void ExpectArrayItemNumber(const array_t *arr, int index, int64_t expected, const char *msg) {
    auto view = lpc::svalue_view::from(&arr->item[index]);
    ASSERT_TRUE(view.is_number());
    EXPECT_EQ(view.number(), expected) << msg;
}

} // namespace

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
    program_t* found_prog = find_function(prog, findstring("add", NULL), &index, &fio, &vio);
    ASSERT_EQ(found_prog, prog) << "find_function did not return the expected program for add().";
    int runtime_index = found_prog->function_table[index].runtime_index + fio;

    push_number(1);
    push_number(2);
    call_function (prog, runtime_index, 2, &ret);

    auto ret_view = lpc::svalue_view::from(&ret);
    EXPECT_TRUE(ret_view.is_number()) << "Expected return type to be integer.";
    EXPECT_EQ(ret_view.number(), 3) << "Expected return value of add(1,2) to be 3.";
    free_prog(prog, 1);
}

TEST_F(LPCInterpreterTest, callInheritedFunction) {
    init_simul_efun("/simul_efun.c"); // need simul efuns to load the inherited object
    ASSERT_NE(simul_efun_ob, nullptr) << "simul_efun_ob is null after init_simul_efun().";
    init_master("/master.c");
    ASSERT_NE(master_ob, nullptr) << "master_ob is null after init_master().";

    object_t* obj = load_object("room/start_room.c", 0); // start_room inherits from base/room.c which defines query_exit()
    ASSERT_NE(obj, nullptr) << "load_object returned null object.";

    shared_str_t method = findstring("query_exit", NULL); // function names are always stored as shared strings
    ASSERT_NE(method, nullptr) << "findstring returned null for `query_exit`.";

    int index, fio, vio;
    svalue_t ret;
    program_t* found_prog = find_function(obj->prog, method, &index, &fio, &vio);
    ASSERT_NE(found_prog, obj->prog) << "find_function did not return inherited program for query_exit().";
    int runtime_index = found_prog->function_table[index].runtime_index + fio;

    current_object = obj;
    variable_index_offset = vio;
    push_constant_string("north");
    call_function (obj->prog, runtime_index, 1, &ret);

    EXPECT_TRUE(lpc::svalue_view::from(&ret).is_string()) << "Expected return value to be a string.";
    EXPECT_STREQ(lpc::svalue_view::from(&ret).c_str(), "room/observatory.c") << "Expected return value of query_exit(\"north\") to be \"room/observatory.c\".";
    free_string_svalue(&ret);
    destruct_object(obj);

    obj = find_object_by_name("/base/room");
    EXPECT_NE(obj, nullptr);
    destruct_object(obj);
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
        program_t* found_prog = find_function(prog, findstring("create", NULL), &index, &fio, &vio);
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
    program_t* found_prog = find_function(prog, findstring("test_utf8_foreach", NULL), &index, &fio, &vio);
    ASSERT_EQ(found_prog, prog) << "find_function did not return the expected program.";
    int runtime_index = found_prog->function_table[index].runtime_index;

    call_function(prog, runtime_index, 0, &ret);

    // Verify the return value is an array
    EXPECT_TRUE(lpc::svalue_view::from(&ret).is_array()) << "Expected return value to be an array.";
    ASSERT_TRUE(ret.u.arr != nullptr) << "Expected non-null array.";
    EXPECT_EQ(ret.u.arr->size, 7) << "Expected array size to be 7.";

    // Verify the array contains the correct Unicode code points:
    // 'H' = 72, 'e' = 101, 'l' = 108, 'l' = 108, 'o' = 111
    // '世' = 0x4E16 (19990), '界' = 0x754C (30028)
    ExpectArrayItemNumber(ret.u.arr, 0, 72, "Expected 'H' (72).");
    ExpectArrayItemNumber(ret.u.arr, 1, 101, "Expected 'e' (101).");
    ExpectArrayItemNumber(ret.u.arr, 2, 108, "Expected 'l' (108).");
    ExpectArrayItemNumber(ret.u.arr, 3, 108, "Expected 'l' (108).");
    ExpectArrayItemNumber(ret.u.arr, 4, 111, "Expected 'o' (111).");
    ExpectArrayItemNumber(ret.u.arr, 5, 19990, "Expected '世' (0x4E16 = 19990).");
    ExpectArrayItemNumber(ret.u.arr, 6, 30028, "Expected '界' (0x754C = 30028).");

    free_svalue(&ret, "test");
    free_prog(prog, 1);
}

#ifdef F_FROM_JSON
TEST_F(LPCInterpreterTest, fromJsonBufferViaLpcVm) {
    /* Compile a small LPC object that calls from_json(buffer) through the
     * full LPC interpreter dispatch path, verifying end-to-end buffer→value. */
    init_simul_efun("/simul_efun.c");
    ASSERT_NE(simul_efun_ob, nullptr) << "simul_efun_ob is null";
    init_master("/master.c");
    ASSERT_NE(master_ob, nullptr) << "master_ob is null";

    program_t *prog = compile_file(-1, "json_buf_test.c",
        "mixed test_from_json_buf(buffer b) { return from_json(b); }\n"
    );
    ASSERT_TRUE(prog != nullptr) << "compile_file returned null";

    int index, fio, vio;
    svalue_t ret;
    program_t *found = find_function(prog, findstring("test_from_json_buf", NULL), &index, &fio, &vio);
    ASSERT_EQ(found, prog);
    int runtime_index = found->function_table[index].runtime_index;

    /* Build buffer holding {\"x\":7} */
    static const char payload[] = "{\"x\":7}";
    buffer_t *buf = allocate_buffer(sizeof(payload) - 1);
    memcpy(buf->item, payload, sizeof(payload) - 1);
    push_refed_buffer(buf);

    eval_cost = CONFIG_INT(__MAX_EVAL_COST__);
    call_function(prog, runtime_index, 1, &ret);

    ASSERT_EQ(ret.type, T_MAPPING) << "Expected T_MAPPING from from_json buffer via LPC VM";
    svalue_t *found_val = find_string_in_mapping(ret.u.map, (char *)"x");
    ASSERT_NE(found_val, &const0u) << "key 'x' not found";
    auto found_view = lpc::svalue_view::from(found_val);
    ASSERT_TRUE(found_view.is_number());
    EXPECT_EQ(found_view.number(), 7);

    free_svalue(&ret, "fromJsonBufferViaLpcVm");
    free_prog(prog, 1);
}
#endif /* F_FROM_JSON */
