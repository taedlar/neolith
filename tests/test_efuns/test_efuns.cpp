#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"
#include "src/apply.h"
#include "lpc/functional.h"
#include "lpc/include/origin.h"

namespace {

void ExpectArrayItemString(const array_t *arr, int index, const char *expected) {
    auto view = lpc::svalue_view::from(&arr->item[index]);
    ASSERT_TRUE(view.is_string());
    ASSERT_STREQ(view.c_str(), expected);
}

void ExpectTopString(const char *expected) {
    auto view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(view.is_string());
    ASSERT_STREQ(view.c_str(), expected);
}

void ExpectTopNumber(int64_t expected) {
    auto view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(view.is_number());
    ASSERT_EQ(view.number(), expected);
}

void ExpectTopReal(double expected) {
    ASSERT_EQ(sp->type, T_REAL);
    ASSERT_DOUBLE_EQ(sp->u.real, expected);
}

void ExpectArrayItemNumber(const array_t *arr, int index, int64_t expected) {
    auto view = lpc::svalue_view::from(&arr->item[index]);
    ASSERT_TRUE(view.is_number());
    ASSERT_EQ(view.number(), expected);
}

} // namespace

TEST_F(EfunsTest, applySlotCallPreservesArgumentOrderAndStack) {
    object_t* obj = load_object("/tests/efuns/test_apply_slot_call", R"(
        mixed *capture(mixed a, mixed b) { return ({ a, b }); }
    )");
    ASSERT_NE(obj, nullptr) << "Failed to load apply slot test object";

    svalue_t *sp_before = sp;
    push_number(11);
    push_number(22);

    svalue_t *ret = APPLY_SLOT_CALL("capture", obj, 2, ORIGIN_DRIVER);
    ASSERT_NE(ret, nullptr) << "APPLY_SLOT_CALL failed";
    ASSERT_EQ(sp, sp_before + 1) << "Expected exactly one slot value on stack after APPLY_SLOT_CALL";

    auto ret_view = lpc::svalue_view::from(ret);
    ASSERT_TRUE(ret_view.is_array()) << "Expected capture() return to be an array";
    ASSERT_NE(ret->u.arr, nullptr);
    ASSERT_EQ(ret->u.arr->size, 2);
    ExpectArrayItemNumber(ret->u.arr, 0, 11);
    ExpectArrayItemNumber(ret->u.arr, 1, 22);

    APPLY_SLOT_FINISH_CALL();
    EXPECT_EQ(sp, sp_before) << "Stack should be restored after APPLY_SLOT_FINISH_CALL";

    destruct_object(obj);
}

TEST_F(EfunsTest, applySlotCallMissingFunctionPreservesSlotContract) {
    object_t* obj = load_object("/tests/efuns/test_apply_slot_missing", R"(
        int sentinel() { return 1; }
    )");
    ASSERT_NE(obj, nullptr) << "Failed to load apply missing-function test object";

    svalue_t *sp_before = sp;
    push_number(42);

    svalue_t *ret = APPLY_SLOT_CALL("does_not_exist", obj, 1, ORIGIN_DRIVER);
    ASSERT_EQ(ret, nullptr) << "Expected null return for missing apply target";

    /* apply_low() pops args on failure; slot placeholder must remain for finish. */
    ASSERT_EQ(sp, sp_before + 1) << "Expected one slot placeholder after failed APPLY_SLOT_CALL";
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_number())
        << "Expected placeholder slot to remain as undefined/number zero";
    ASSERT_EQ(lpc::svalue_view::from(sp).number(), 0);

    APPLY_SLOT_FINISH_CALL();
    EXPECT_EQ(sp, sp_before) << "Stack should be restored after APPLY_SLOT_FINISH_CALL on failure path";

    destruct_object(obj);
}

TEST_F(EfunsTest, applySlotSafeCallMissingFunctionPreservesSlotContract) {
    object_t* obj = load_object("/tests/efuns/test_apply_slot_safe_missing", R"(
        int sentinel() { return 1; }
    )");
    ASSERT_NE(obj, nullptr) << "Failed to load apply safe-missing test object";

    svalue_t *sp_before = sp;
    push_number(7);

    svalue_t *ret = APPLY_SLOT_SAFE_CALL("does_not_exist", obj, 1, ORIGIN_DRIVER);
    ASSERT_EQ(ret, nullptr) << "Expected null return for missing safe apply target";

    ASSERT_EQ(sp, sp_before + 1) << "Expected one slot placeholder after failed APPLY_SLOT_SAFE_CALL";
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_number())
        << "Expected placeholder slot to remain as undefined/number zero";
    ASSERT_EQ(lpc::svalue_view::from(sp).number(), 0);

    APPLY_SLOT_FINISH_CALL();
    EXPECT_EQ(sp, sp_before) << "Stack should be restored after APPLY_SLOT_FINISH_CALL on safe failure path";

    destruct_object(obj);
}

TEST_F(EfunsTest, applySlotSafeCallNullObjectPreservesSlotContract) {
    svalue_t *sp_before = sp;
    push_number(13);

    svalue_t *ret = APPLY_SLOT_SAFE_CALL("does_not_matter", nullptr, 1, ORIGIN_DRIVER);
    ASSERT_EQ(ret, nullptr) << "Expected null return for safe apply on null object";

    ASSERT_EQ(sp, sp_before + 1) << "Expected one slot placeholder after null-object safe apply";
    auto top = lpc::svalue_view::from(sp);
    ASSERT_TRUE(top.is_number()) << "Expected placeholder slot to remain as undefined/number zero";
    ASSERT_EQ(top.number(), 0);

    APPLY_SLOT_FINISH_CALL();
    EXPECT_EQ(sp, sp_before) << "Stack should be restored after APPLY_SLOT_FINISH_CALL on null-object path";
}

TEST_F(EfunsTest, applySlotSafeCallDestructedObjectPreservesSlotContract) {
    object_t* obj = load_object("/tests/efuns/test_apply_slot_safe_destructed", R"(
        int sentinel() { return 1; }
    )");
    ASSERT_NE(obj, nullptr) << "Failed to load destructed-object test object";

    destruct_object(obj);

    svalue_t *sp_before = sp;
    push_number(21);

    svalue_t *ret = APPLY_SLOT_SAFE_CALL("sentinel", obj, 1, ORIGIN_DRIVER);
    ASSERT_EQ(ret, nullptr) << "Expected null return for safe apply on destructed object";

    ASSERT_EQ(sp, sp_before + 1) << "Expected one slot placeholder after destructed-object safe apply";
    auto top = lpc::svalue_view::from(sp);
    ASSERT_TRUE(top.is_number()) << "Expected placeholder slot to remain as undefined/number zero";
    ASSERT_EQ(top.number(), 0);

    APPLY_SLOT_FINISH_CALL();
    EXPECT_EQ(sp, sp_before) << "Stack should be restored after APPLY_SLOT_FINISH_CALL on destructed-object path";
}

TEST_F(EfunsTest, applySlotSafeMasterCallMissingMasterPreservesSlotContract) {
    object_t *saved_master = master_ob;

    svalue_t *sp_before = sp;
    push_number(34);

    master_ob = nullptr;
    svalue_t *ret = APPLY_SLOT_SAFE_MASTER_CALL("does_not_matter", 1);
    master_ob = saved_master;

    ASSERT_EQ(ret, (svalue_t *)-1) << "Expected safe master apply to return -1 when master object is missing";
    ASSERT_EQ(sp, sp_before + 1) << "Expected one slot placeholder after missing-master safe apply";
    auto top = lpc::svalue_view::from(sp);
    ASSERT_TRUE(top.is_number()) << "Expected placeholder slot to remain as undefined/number zero";
    ASSERT_EQ(top.number(), 0);

    APPLY_SLOT_FINISH_CALL();
    EXPECT_EQ(sp, sp_before) << "Stack should be restored after APPLY_SLOT_FINISH_CALL on missing-master path";
}

TEST_F(EfunsTest, safeFunctionPointerSlotCallSaveContextFailurePreservesSlotContract) {
    object_t* obj = load_object("/tests/efuns/test_funp_slot_save_context", R"(
        int sentinel(int value) { return value; }
    )");
    ASSERT_NE(obj, nullptr) << "Failed to load function pointer save_context test object";

    object_t *saved_current_object = current_object;
    current_object = obj;
    funptr_t *funp = make_lfun_funp_by_name("sentinel", &const0);
    ASSERT_NE(funp, nullptr) << "Failed to create local function pointer";

    control_stack_t *saved_csp = csp;
    svalue_t *sp_before = sp;
    push_number(55);

    csp = &control_stack[CONFIG_INT(__MAX_CALL_DEPTH__) - 1];
    svalue_t *ret = SAFE_CALL_FUNCTION_POINTER_SLOT_CALL(funp, 1);
    csp = saved_csp;

    EXPECT_EQ(ret, nullptr) << "Expected safe function pointer call to return null when save_context fails";
    EXPECT_EQ(sp, sp_before + 1) << "Expected one slot placeholder after save_context failure";
    auto top = lpc::svalue_view::from(sp);
    EXPECT_TRUE(top.is_number()) << "Expected placeholder slot to remain as undefined/number zero";
    EXPECT_EQ(top.number(), 0);

    CALL_FUNCTION_POINTER_SLOT_FINISH();
    EXPECT_EQ(sp, sp_before) << "Stack should be restored after CALL_FUNCTION_POINTER_SLOT_FINISH on save_context failure";

    free_funp(funp);
    current_object = saved_current_object;
    destruct_object(obj);
}

TEST_F(EfunsTest, evaluateEfunReturnsExpressionValue) {
    object_t* obj = load_object("/tests/efuns/test_evaluate_contract", R"(
        int plus1(int x) { return x + 1; }
        int run_eval() { return evaluate((: plus1 :), 41); }
    )");
    ASSERT_NE(obj, nullptr) << "Failed to load evaluate() contract test object";

    svalue_t *sp_before = sp;
    svalue_t *ret = APPLY_SLOT_CALL("run_eval", obj, 0, ORIGIN_DRIVER);
    ASSERT_NE(ret, nullptr) << "run_eval apply failed";
    ASSERT_EQ(sp, sp_before + 1) << "Expected one slot value after run_eval";

    auto view = lpc::svalue_view::from(ret);
    ASSERT_TRUE(view.is_number()) << "evaluate() should return the expression value";
    ASSERT_EQ(view.number(), 42);

    APPLY_SLOT_FINISH_CALL();
    EXPECT_EQ(sp, sp_before) << "Stack should be restored after APPLY_SLOT_FINISH_CALL";

    destruct_object(obj);
}

TEST_F(EfunsTest, functionPointerSlotCallErrorPathKeepsRuntimeUsable) {
    object_t* obj = load_object("/tests/efuns/test_funp_slot_call", R"(
        mixed *capture(mixed a, mixed b) { return ({ a, b }); }
        void explode() { error("funp explode"); }
    )");
    ASSERT_NE(obj, nullptr) << "Failed to load function pointer slot test object";

    current_object = obj;
    funptr_t *capture_funp = make_lfun_funp_by_name("capture", &const0);
    funptr_t *explode_funp = make_lfun_funp_by_name("explode", &const0);
    ASSERT_NE(capture_funp, nullptr);
    ASSERT_NE(explode_funp, nullptr);

    svalue_t *sp_before = sp;
    push_number(3);
    push_number(4);

    svalue_t *ret = CALL_FUNCTION_POINTER_SLOT_CALL(capture_funp, 2);
    ASSERT_NE(ret, nullptr);
    ASSERT_EQ(sp, sp_before + 1) << "Expected one slot value after successful function pointer call";

    auto ret_view = lpc::svalue_view::from(ret);
    ASSERT_TRUE(ret_view.is_array());
    ASSERT_EQ(ret->u.arr->size, 2);
    ExpectArrayItemNumber(ret->u.arr, 0, 3);
    ExpectArrayItemNumber(ret->u.arr, 1, 4);

    CALL_FUNCTION_POINTER_SLOT_FINISH();
    ASSERT_EQ(sp, sp_before) << "Stack should be restored after successful slot finish";

    error_context_t econ;
    volatile int caught = 0;
    save_context(&econ);
    try {
        CALL_FUNCTION_POINTER_SLOT_CALL(explode_funp, 0);
        FAIL() << "Expected explode() to raise a runtime error";
    }
    catch (const neolith::driver_runtime_error &) {
        caught = 1;
        restore_context(&econ);
    }
    pop_context(&econ);
    ASSERT_EQ(caught, 1) << "Expected runtime error from function pointer call";
    ASSERT_EQ(sp, sp_before) << "Stack should remain valid after function pointer error";

    push_number(8);
    push_number(9);
    ret = CALL_FUNCTION_POINTER_SLOT_CALL(capture_funp, 2);
    ASSERT_NE(ret, nullptr) << "Function pointer should still be callable after prior error";
    ASSERT_TRUE(lpc::svalue_view::from(ret).is_array());
    ASSERT_EQ(ret->u.arr->size, 2);
    ExpectArrayItemNumber(ret->u.arr, 0, 8);
    ExpectArrayItemNumber(ret->u.arr, 1, 9);
    CALL_FUNCTION_POINTER_SLOT_FINISH();
    EXPECT_EQ(sp, sp_before);

    free_funp(capture_funp);
    free_funp(explode_funp);
    destruct_object(obj);
}

TEST_F(EfunsTest, throwError) {
    error_context_t econ;
    save_context (&econ);
    try {
        // do efun throw() without catching it.
        push_constant_string("Error thrown by efun throw()");
        f_throw();
        FAIL() << "Efun throw() did not throw an LPC error as expected.";
    }
    catch (const neolith::driver_runtime_error &) {
        restore_context (&econ);
        debug_message("***** expected: caught error raised by efun throw()");
    }
    pop_context (&econ);
}

TEST_F(EfunsTest, throwWithoutCatchRaisesRuntimeError) {
    // Compile LPC code that throws without a catch boundary.
    // This validates only that throw() outside catch() raises the runtime error path.
    program_t* prog = compile_file(-1, "throw_no_catch.c",
        "void throw_without_catch() { throw(\"test payload\"); }\n"
    );
    ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";

    int index = 0, fio = 0, vio = 0;
    program_t* found_prog = find_function(prog, findstring("throw_without_catch", NULL), &index, &fio, &vio);
    ASSERT_EQ(found_prog, prog) << "find_function failed for throw_without_catch.";
    int runtime_index = found_prog->function_table[index].runtime_index + fio;

    error_context_t econ;
    volatile int raised = 0;
    save_context(&econ);
    try {
        eval_cost = CONFIG_INT(__MAX_EVAL_COST__);
        call_function(prog, runtime_index, 0, nullptr);
    }
    catch (const neolith::driver_runtime_error &) {
        raised = 1;
        restore_context(&econ);
    }
    pop_context(&econ);
    free_prog(prog, 1);

    EXPECT_EQ(raised, 1) << "throw() outside catch() must raise runtime error path.";
}

TEST_F(EfunsTest, errorHandlerCallableContract) {
    // Verify that error_handler master apply is called during error propagation.
    // The test injects a custom error_handler via pretext, verifies it's in the
    // built master, and confirms errors propagate through the error_handler path.

    // Create a master object with a custom error_handler function via pretext.
    // This demonstrates init_master now accepts pre_text for test instrumentation.
    const char *master_pretext =
        "#pragma strict_types\n"
        "void error_handler(mapping error, int caught) {\n"
        "  // Custom error_handler injected via pretext.\n"
        "  // The driver routes all runtime errors to this function.\n"
        "}\n";

    // Load master with custom error_handler injected.
    init_master("/master.c", master_pretext);
    ASSERT_NE(master_ob, nullptr) << "master_ob is null after init_master().";

    // Verify the custom error_handler exists in the compiled master.
    // This proves pretext was successfully compiled into the master.
    int eh_index = 0, eh_fio = 0, eh_vio = 0;
    program_t* master_prog = master_ob->prog;
    program_t* found_eh = find_function(master_prog, findstring("error_handler", NULL), &eh_index, &eh_fio, &eh_vio);
    ASSERT_EQ(found_eh, master_prog) << "error_handler not found in master (pretext not injected).";

    // Compile and execute LPC code that calls error().
    program_t* prog = compile_file(-1, "error_trigger.c",
        "void trigger_error() { error(\"Test error message\"); }\n"
    );
    ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";

    int index = 0, fio = 0, vio = 0;
    program_t* found_prog = find_function(prog, findstring("trigger_error", NULL), &index, &fio, &vio);
    ASSERT_EQ(found_prog, prog) << "find_function failed for trigger_error.";
    int runtime_index = found_prog->function_table[index].runtime_index + fio;

    // Execute error() and verify error propagates through error handler.
    error_context_t econ;
    volatile int error_caught = 0;
    save_context(&econ);
    try {
        eval_cost = CONFIG_INT(__MAX_EVAL_COST__);
        call_function(prog, runtime_index, 0, nullptr);
    }
    catch (const neolith::driver_runtime_error &) {
        error_caught = 1;
        restore_context(&econ);
    }
    pop_context(&econ);
    free_prog(prog, 1);

    EXPECT_EQ(error_caught, 1) << "error() must propagate through error_handler path.";
}

TEST_F(EfunsTest, stringCaseConversion) {
    // Test upper_case, lower_case, and capitalize efuns
    push_constant_string("Hello World!");

    f_upper_case();
    ExpectTopString("HELLO WORLD!");

    f_lower_case();
    ExpectTopString("hello world!");

    f_capitalize();
    ExpectTopString("Hello world!");
}

TEST_F(EfunsTest, floatFunctions) {
    push_constant_string("3.14159");
    f_to_float();
    ExpectTopReal(3.14159);

    push_number(42);
    f_to_float();
    ExpectTopReal(42.0);

    f_floatp();
    ExpectTopNumber(1); // true for float

    pop_n_elems(1);
    push_constant_string("Not a float");
    f_floatp();
    ExpectTopNumber(0); // false for not a float
}

TEST_F(EfunsTest, stringExplode) {
    // Test explode efun
    push_constant_string("Hello World!");
    push_constant_string(" ");
    f_explode();
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_array());
    ASSERT_EQ(sp->u.arr->size, 2);
    ExpectArrayItemString(sp->u.arr, 0, "Hello");
    ExpectArrayItemString(sp->u.arr, 1, "World!");

    // Explode with empty delimiter string (Neolith extension: splits into wide characters)
    push_constant_string("こんにちは");
    push_constant_string("");
    f_explode();
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_array());
    ASSERT_EQ(sp->u.arr->size, 5);
    ExpectArrayItemString(sp->u.arr, 0, "こ");
    ExpectArrayItemString(sp->u.arr, 1, "ん");
    ExpectArrayItemString(sp->u.arr, 2, "に");
    ExpectArrayItemString(sp->u.arr, 3, "ち");
    ExpectArrayItemString(sp->u.arr, 4, "は");

    // Explode with multibyte delimiter (Neolith extension: does not break multibyte characters)
    push_constant_string("小星星");
    push_constant_string("\xE5\xB0\x8F"); // "小" in UTF-8
    f_explode();
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_array());
    ASSERT_EQ(sp->u.arr->size, 2);
    ExpectArrayItemString(sp->u.arr, 0, "");
    ExpectArrayItemString(sp->u.arr, 1, "星星");
    push_constant_string("小星星");
    push_constant_string("\xE5\xB0"); // partial sequence of "小" in UTF-8
    f_explode();
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_array());
    ASSERT_EQ(sp->u.arr->size, 1);
    ExpectArrayItemString(sp->u.arr, 0, "小星星"); // delimiter not found, not even at character boundary
}

TEST_F(EfunsTest, stringExplodeUtf8) {
    // Test explode efun with UTF-8 multibyte characters
    // When delimiter is empty, explode should split into individual Unicode characters
    // Each character should be returned as a multibyte UTF-8 string
    
    // Test with mixed ASCII and UTF-8: "Hello世界" 
    // Using hex escapes to ensure proper encoding: Hello\xe4\xb8\x96\xe7\x95\x8c
    push_constant_string("Hello\xe4\xb8\x96\xe7\x95\x8c");
    push_constant_string("");
    f_explode();
    
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_array()) << "Expected array result from explode";
    ASSERT_EQ(sp->u.arr->size, 7) << "Expected 7 characters: 5 ASCII + 2 UTF-8";
    
    // Verify ASCII characters (single bytes)
    ASSERT_TRUE(lpc::svalue_view::from(&sp->u.arr->item[0]).is_string());
    ExpectArrayItemString(sp->u.arr, 0, "H");
    ASSERT_TRUE(lpc::svalue_view::from(&sp->u.arr->item[1]).is_string());
    ExpectArrayItemString(sp->u.arr, 1, "e");
    ASSERT_TRUE(lpc::svalue_view::from(&sp->u.arr->item[2]).is_string());
    ExpectArrayItemString(sp->u.arr, 2, "l");
    ASSERT_TRUE(lpc::svalue_view::from(&sp->u.arr->item[3]).is_string());
    ExpectArrayItemString(sp->u.arr, 3, "l");
    ASSERT_TRUE(lpc::svalue_view::from(&sp->u.arr->item[4]).is_string());
    ExpectArrayItemString(sp->u.arr, 4, "o");
    
    // Verify UTF-8 multibyte characters (3 bytes each for Chinese characters)
    ASSERT_TRUE(lpc::svalue_view::from(&sp->u.arr->item[5]).is_string());
    ASSERT_STREQ(lpc::svalue_view::from(&sp->u.arr->item[5]).c_str(), "\xe4\xb8\x96") << "Expected UTF-8 '世'";
    
    ASSERT_TRUE(lpc::svalue_view::from(&sp->u.arr->item[6]).is_string());
    ASSERT_STREQ(lpc::svalue_view::from(&sp->u.arr->item[6]).c_str(), "\xe7\x95\x8c") << "Expected UTF-8 '界'";
    
    pop_stack(); // Clean up the result array
    
    // Test with only UTF-8 characters: "日本語" (Japanese)
    push_constant_string("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");
    push_constant_string("");
    f_explode();
    
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_array());
    ASSERT_EQ(sp->u.arr->size, 3) << "Expected 3 Japanese characters";
    
    ExpectArrayItemString(sp->u.arr, 0, "\xe6\x97\xa5"); // '日'
    ExpectArrayItemString(sp->u.arr, 1, "\xe6\x9c\xac"); // '本'
    ExpectArrayItemString(sp->u.arr, 2, "\xe8\xaa\x9e"); // '語'
    
    pop_stack();
}
