/*
 * test_json.cpp — unit tests for to_json / from_json efuns.
 *
 * All test bodies are guarded by F_TO_JSON / F_FROM_JSON (generated opcode
 * macros present only when PACKAGE_JSON=ON at cmake configure time).  The
 * file always compiles but the test suite is empty unless the efuns are
 * enabled.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

extern "C" {
    #include "lpc/mapping.h"
    #include "src/stralloc.h"
    #include "src/error_context.h"
}

/* ------------------------------------------------------------------ */
/* to_json tests                                                       */
/* ------------------------------------------------------------------ */

#ifdef F_TO_JSON

TEST_F(EfunsTest, toJsonInteger) {
    push_number(42);
    f_to_json();
    ASSERT_EQ(sp->type, T_STRING);
    EXPECT_STREQ(sp->u.string, "42");
}

TEST_F(EfunsTest, toJsonNegativeInteger) {
    push_number(-7);
    f_to_json();
    ASSERT_EQ(sp->type, T_STRING);
    EXPECT_STREQ(sp->u.string, "-7");
}

TEST_F(EfunsTest, toJsonFloat) {
    /* Boost.JSON serializes doubles in its own short-form notation */
    push_real(1.5);
    f_to_json();
    ASSERT_EQ(sp->type, T_STRING);
    EXPECT_STREQ(sp->u.string, "1.5E0");
}

TEST_F(EfunsTest, toJsonString) {
    push_constant_string("hello");
    f_to_json();
    ASSERT_EQ(sp->type, T_STRING);
    EXPECT_STREQ(sp->u.string, "\"hello\"");
}

TEST_F(EfunsTest, toJsonStringEscaping) {
    /* Verify JSON escaping of quotes and backslash */
    push_constant_string("say \"hi\"");
    f_to_json();
    ASSERT_EQ(sp->type, T_STRING);
    EXPECT_STREQ(sp->u.string, "\"say \\\"hi\\\"\"");
}

TEST_F(EfunsTest, toJsonUndefined) {
    /* undefined (subtype T_UNDEFINED) maps to JSON null */
    push_undefined();
    f_to_json();
    ASSERT_EQ(sp->type, T_STRING);
    EXPECT_STREQ(sp->u.string, "null");
}

TEST_F(EfunsTest, toJsonArray) {
    /* Build [1, 2, 3] and verify JSON serialization */
    array_t *arr = allocate_array(3);
    arr->item[0].u.number = 1;
    arr->item[1].u.number = 2;
    arr->item[2].u.number = 3;
    /* All items initialized with T_NUMBER / subtype 0 by allocate_array; just set u.number */
    push_refed_array(arr); /* transfer ownership to stack (ref already 1) */
    f_to_json();
    ASSERT_EQ(sp->type, T_STRING);
    EXPECT_STREQ(sp->u.string, "[1,2,3]");
}

TEST_F(EfunsTest, toJsonMapping) {
    /* Build {"k": 99} and verify JSON serialization */
    mapping_t *m = allocate_mapping(1);
    svalue_t key;
    key.type = T_STRING;
    key.subtype = STRING_CONSTANT;
    key.u.string = (char *)"k";
    svalue_t *val = find_for_insert(m, &key, 1);
    /* find_for_insert converts STRING_CONSTANT to STRING_SHARED via svalue_to_int;
     * key.u.string now points to the shared string — release the extra ref. */
    free_string(key.u.string);
    val->type = T_NUMBER;
    val->subtype = 0;
    val->u.number = 99;
    push_refed_mapping(m); /* transfer ownership to stack */
    f_to_json();
    ASSERT_EQ(sp->type, T_STRING);
    EXPECT_STREQ(sp->u.string, "{\"k\":99}");
}

TEST_F(EfunsTest, toJsonNonStringKeyError) {
    /* Mapping with an integer key must raise a runtime error */
    mapping_t *m = allocate_mapping(1);
    svalue_t key;
    key.type = T_NUMBER;
    key.subtype = 0;
    key.u.number = 1;
    svalue_t *val = find_for_insert(m, &key, 1);
    val->type = T_NUMBER;
    val->subtype = 0;
    val->u.number = 5;

    bool error_raised = false;
    error_context_t econ;
    save_context(&econ); /* save before push so restore_context unwinds it */
    if (setjmp(econ.context)) {
        restore_context(&econ);
        error_raised = true;
    }
    else {
        push_refed_mapping(m);
        f_to_json();
        FAIL() << "to_json with non-string key should have raised an error.";
    }
    pop_context(&econ);
    EXPECT_TRUE(error_raised);
}

#endif /* F_TO_JSON */

/* ------------------------------------------------------------------ */
/* from_json tests                                                     */
/* ------------------------------------------------------------------ */

#ifdef F_FROM_JSON

TEST_F(EfunsTest, fromJsonInteger) {
    copy_and_push_string("42");
    f_from_json();
    ASSERT_EQ(sp->type, T_NUMBER);
    EXPECT_EQ(sp->u.number, 42);
}

TEST_F(EfunsTest, fromJsonNegativeInteger) {
    copy_and_push_string("-7");
    f_from_json();
    ASSERT_EQ(sp->type, T_NUMBER);
    EXPECT_EQ(sp->u.number, -7);
}

TEST_F(EfunsTest, fromJsonFloat) {
    copy_and_push_string("1.5");
    f_from_json();
    ASSERT_EQ(sp->type, T_REAL);
    EXPECT_DOUBLE_EQ(sp->u.real, 1.5);
}

TEST_F(EfunsTest, fromJsonString) {
    copy_and_push_string("\"hello\"");
    f_from_json();
    ASSERT_EQ(sp->type, T_STRING);
    EXPECT_STREQ(sp->u.string, "hello");
}

TEST_F(EfunsTest, fromJsonNull) {
    /* JSON null maps to T_NUMBER 0 with subtype T_UNDEFINED (same as what undefinedp() checks) */
    copy_and_push_string("null");
    f_from_json();
    ASSERT_EQ(sp->type, T_NUMBER);
    EXPECT_EQ(sp->subtype, T_UNDEFINED);
}

TEST_F(EfunsTest, fromJsonBoolTrue) {
    copy_and_push_string("true");
    f_from_json();
    ASSERT_EQ(sp->type, T_NUMBER);
    EXPECT_EQ(sp->u.number, 1);
}

TEST_F(EfunsTest, fromJsonBoolFalse) {
    copy_and_push_string("false");
    f_from_json();
    ASSERT_EQ(sp->type, T_NUMBER);
    EXPECT_EQ(sp->u.number, 0);
}

TEST_F(EfunsTest, fromJsonArray) {
    copy_and_push_string("[1,2,3]");
    f_from_json();
    ASSERT_EQ(sp->type, T_ARRAY);
    ASSERT_EQ((int)sp->u.arr->size, 3);
    EXPECT_EQ(sp->u.arr->item[0].type, T_NUMBER);
    EXPECT_EQ(sp->u.arr->item[0].u.number, 1);
    EXPECT_EQ(sp->u.arr->item[1].u.number, 2);
    EXPECT_EQ(sp->u.arr->item[2].u.number, 3);
}

TEST_F(EfunsTest, fromJsonObject) {
    copy_and_push_string("{\"a\":1}");
    f_from_json();
    ASSERT_EQ(sp->type, T_MAPPING);
    /* find_string_in_mapping returns &const0u for missing keys; check value type and number */
    svalue_t *found = find_string_in_mapping(sp->u.map, (char *)"a");
    ASSERT_NE(found, &const0u) << "key 'a' not found in from_json result mapping";
    EXPECT_EQ(found->type, T_NUMBER);
    EXPECT_EQ(found->u.number, 1);
}

TEST_F(EfunsTest, fromJsonInvalidError) {
    bool error_raised = false;
    error_context_t econ;
    save_context(&econ);
    if (setjmp(econ.context)) {
        restore_context(&econ);
        error_raised = true;
    }
    else {
        copy_and_push_string("{ invalid");
        f_from_json();
        FAIL() << "from_json with invalid JSON should have raised an error.";
    }
    pop_context(&econ);
    EXPECT_TRUE(error_raised);
}

#endif /* F_FROM_JSON */

/* ------------------------------------------------------------------ */
/* Round-trip tests                                                    */
/* ------------------------------------------------------------------ */

#if defined(F_TO_JSON) && defined(F_FROM_JSON)

TEST_F(EfunsTest, roundTripInteger) {
    push_number(12345);
    f_to_json();
    ASSERT_EQ(sp->type, T_STRING);
    f_from_json();
    ASSERT_EQ(sp->type, T_NUMBER);
    EXPECT_EQ(sp->u.number, 12345);
}

TEST_F(EfunsTest, roundTripString) {
    copy_and_push_string("hello world");
    f_to_json();
    ASSERT_EQ(sp->type, T_STRING);
    f_from_json();
    ASSERT_EQ(sp->type, T_STRING);
    EXPECT_STREQ(sp->u.string, "hello world");
}

TEST_F(EfunsTest, roundTripNull) {
    /* to_json(undefined) → "null" */
    push_undefined();
    f_to_json();
    ASSERT_EQ(sp->type, T_STRING);
    EXPECT_STREQ(sp->u.string, "null");

    /* from_json("null") → T_NUMBER 0 subtype T_UNDEFINED */
    copy_and_push_string("null");
    f_from_json();
    ASSERT_EQ(sp->type, T_NUMBER);
    EXPECT_EQ(sp->subtype, T_UNDEFINED);
}

TEST_F(EfunsTest, roundTripArray) {
    array_t *arr = allocate_array(2);
    arr->item[0].u.number = 10;
    arr->item[1].u.number = 20;
    push_refed_array(arr);
    f_to_json();
    ASSERT_EQ(sp->type, T_STRING);
    f_from_json();
    ASSERT_EQ(sp->type, T_ARRAY);
    ASSERT_EQ((int)sp->u.arr->size, 2);
    EXPECT_EQ(sp->u.arr->item[0].u.number, 10);
    EXPECT_EQ(sp->u.arr->item[1].u.number, 20);
}

TEST_F(EfunsTest, roundTripFloat) {
    /* Verify the float round-trip regardless of Boost.JSON serialization format */
    push_real(3.14);
    f_to_json();
    ASSERT_EQ(sp->type, T_STRING);
    f_from_json();
    ASSERT_EQ(sp->type, T_REAL);
    EXPECT_DOUBLE_EQ(sp->u.real, 3.14);
}

#endif /* F_TO_JSON && F_FROM_JSON */
