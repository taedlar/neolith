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

#include <string>
#include "fixtures.hpp"

extern "C" {
    #include "lpc/buffer.h"
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
    EXPECT_DOUBLE_EQ(strtod(sp->u.string, nullptr), 1.5);
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
    key.u.const_string = "k";
    svalue_t *val = find_for_insert(m, &key, 1);
    /* find_for_insert converts STRING_CONSTANT to STRING_SHARED via svalue_to_int;
     * key.u.shared_string now points to the shared string — release the extra ref. */
    free_string(key.u.shared_string);
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

TEST_F(EfunsTest, fromJsonBuffer) {
    static const char payload[] = "{\"a\":1}";
    buffer_t *buf = allocate_buffer(sizeof(payload) - 1);
    memcpy(buf->item, payload, sizeof(payload) - 1);
    push_refed_buffer(buf);

    f_from_json();

    ASSERT_EQ(sp->type, T_MAPPING);
    svalue_t *found = find_string_in_mapping(sp->u.map, (char *)"a");
    ASSERT_NE(found, &const0u) << "key 'a' not found in from_json buffer result mapping";
    EXPECT_EQ(found->type, T_NUMBER);
    EXPECT_EQ(found->u.number, 1);
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

TEST_F(EfunsTest, fromJsonInvalidBufferError) {
    bool error_raised = false;
    error_context_t econ;
    save_context(&econ);
    if (setjmp(econ.context)) {
        restore_context(&econ);
        error_raised = true;
    }
    else {
        static const char payload[] = "{ invalid";
        buffer_t *buf = allocate_buffer(sizeof(payload) - 1);
        memcpy(buf->item, payload, sizeof(payload) - 1);
        push_refed_buffer(buf);
        f_from_json();
        FAIL() << "from_json with invalid JSON buffer should have raised an error.";
    }
    pop_context(&econ);
    EXPECT_TRUE(error_raised);
}

TEST_F(EfunsTest, fromJsonLargeBuffer) {
    /* Build a JSON object with a small number of keys whose total serialized
     * length exceeds 65535 bytes (the old unsigned-short buffer-size limit).
     * This proves:
     *   (a) buf->size is stored as unsigned int (the (unsigned short) cast bugfix),
     *   (b) the buffer path passes the correct byte count to Boost.JSON.
     *
     * We keep key/value counts within MaxArraySize (15000) and each string value
     * within the driver's stralloc limits, so no other limits are hit. */
    const int NKEYS = 100;
    const int VALUE_LEN = 800;  /* bytes per string value */
    /* Total JSON ≈ NKEYS * (VALUE_LEN + key overhead) ≈ 100 * 820 = ~82 KB > 65535 */

    std::string json;
    json.reserve(NKEYS * (VALUE_LEN + 20));
    json += '{';
    for (int i = 0; i < NKEYS; i++) {
        if (i > 0) json += ',';
        json += '"';
        char keybuf[16];
        snprintf(keybuf, sizeof(keybuf), "k%03d", i);
        json += keybuf;
        json += "\":\"";
        json.append(VALUE_LEN, 'x');
        json += '"';
    }
    json += '}';

    ASSERT_GT(json.size(), 65535u)
        << "JSON payload must exceed 65535 to exercise unsigned-int buffer size";

    buffer_t *buf = allocate_buffer(json.size());
    memcpy(buf->item, json.data(), json.size());
    push_refed_buffer(buf);

    f_from_json();

    ASSERT_EQ(sp->type, T_MAPPING);
    /* Spot-check first and last key */
    svalue_t *v0 = find_string_in_mapping(sp->u.map, (char *)"k000");
    ASSERT_NE(v0, &const0u) << "key 'k000' not found";
    EXPECT_EQ(v0->type, T_STRING);
    EXPECT_EQ(strlen(v0->u.string), (size_t)VALUE_LEN);
    char lastkey[16];
    snprintf(lastkey, sizeof(lastkey), "k%03d", NKEYS - 1);
    svalue_t *vlast = find_string_in_mapping(sp->u.map, lastkey);
    ASSERT_NE(vlast, &const0u) << "last key not found";
    EXPECT_EQ(vlast->type, T_STRING);
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
