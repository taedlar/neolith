#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include "std.h"
#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include "lpc/operator.h"

// Test fixture providing VM and string allocation context
class StringOperatorsTest : public ::testing::Test {
protected:
    void SetUp() override {
        debug_set_log_with_date(0);
        setlocale(LC_ALL, PLATFORM_UTF8_LOCALE);
        init_stem(3, 0177, "");
        MAIN_OPTION(pedantic) = 1;
        debug_message("[ SETUP    ] CTEST_FULL_OUTPUT");

        // Initialize string allocation subsystem
        init_strings(65536, 1024 * 1024);
    }

    void TearDown() override {
        deinit_strings();
    }

    // Helper: Verify svalue content and type
    void assert_string_content(svalue_t *sv, const char *expected, size_t expected_len,
                               unsigned short expected_subtype) {
        ASSERT_FALSE(sv == nullptr);
        lpc::svalue_view view = lpc::svalue_view::from(sv);
        ASSERT_TRUE(view.is_string());
        ASSERT_EQ(sv->subtype, expected_subtype);
        size_t actual_len = view.length();
        ASSERT_EQ(actual_len, expected_len);
        ASSERT_TRUE(memcmp(view.c_str(), expected, expected_len) == 0);
    }
};

// === EXTEND_SVALUE_STRING_LEN Tests ===

TEST_F(StringOperatorsTest, ExtendViaLenAppendNormal) {
    // Test appending to a malloc string with explicit length
    lpc::svalue target;
    target.set_malloc_string("Hello");

    const char *suffix = " World";
    size_t suffix_len = 6;

    EXTEND_SVALUE_STRING_LEN(target.raw(), suffix, suffix_len, "test");

    assert_string_content(target.raw(), "Hello World", 11, STRING_MALLOC);
}

TEST_F(StringOperatorsTest, ExtendViaLenAppendEmpty) {
    // Test appending empty string (no-op)
    lpc::svalue target;
    target.set_malloc_string("Test");

    EXTEND_SVALUE_STRING_LEN(target.raw(), "", 0, "test");

    assert_string_content(target.raw(), "Test", 4, STRING_MALLOC);
}

TEST_F(StringOperatorsTest, ExtendViaLenAppendToEmpty) {
    // Test appending to empty string
    lpc::svalue target;
    target.set_malloc_string("");

    EXTEND_SVALUE_STRING_LEN(target.raw(), "Data", 4, "test");

    assert_string_content(target.raw(), "Data", 4, STRING_MALLOC);
}

TEST_F(StringOperatorsTest, ExtendViaLenAppendWithEmbeddedNul) {
    // Test appending string with embedded NUL
    lpc::svalue target;
    target.set_malloc_string("Start");

    const char suffix[] = "A\0B";  // 3 bytes: A, NUL, B
    EXTEND_SVALUE_STRING_LEN(target.raw(), suffix, 3, "test");

    // Result should be "StartA\0B" (8 bytes)
    assert_string_content(target.raw(), "StartA\0B", 8, STRING_MALLOC);
}

TEST_F(StringOperatorsTest, ExtendViaLenMallocSelfReuse) {
    // Test extending a malloc string with ref count 1 (in-place extension)
    lpc::svalue target;
    target.set_malloc_string("Hello");

    // Verify it can be extended in place
    EXTEND_SVALUE_STRING_LEN(target.raw(), " Again", 6, "test");

    assert_string_content(target.raw(), "Hello Again", 11, STRING_MALLOC);
}

// === SVALUE_STRING_ADD_LEFT_LEN Tests ===

TEST_F(StringOperatorsTest, AddLeftViaLenPrependNormal) {
    // Test prepending to stack-top string through the actual macro path.
    svalue_t stack[2] = {};
    lpc::svalue_view::from(&stack[1]).set_malloc_string("World");
    svalue_t *saved_sp = sp;
    sp = &stack[1];

    const char *prefix = "Hello ";
    size_t prefix_len = 6;

    SVALUE_STRING_ADD_LEFT_LEN(prefix, prefix_len, "test");

    ASSERT_EQ(sp, &stack[0]);
    assert_string_content(sp, "Hello World", 11, STRING_MALLOC);

    free_svalue(sp, "AddLeftViaLenPrependNormal");
    sp = saved_sp;
}

TEST_F(StringOperatorsTest, AddLeftViaLenWithEmbeddedNul) {
    // Test prepending string with embedded NUL
    svalue_t stack[2] = {};
    lpc::svalue_view::from(&stack[1]).set_malloc_string("End");
    svalue_t *saved_sp = sp;
    sp = &stack[1];

    const char prefix[] = "A\0B";  // 3 bytes with embedded NUL

    SVALUE_STRING_ADD_LEFT_LEN(prefix, 3, "test");

    ASSERT_EQ(sp, &stack[0]);
    assert_string_content(sp, "A\0BEnd", 6, STRING_MALLOC);

    free_svalue(sp, "AddLeftViaLenWithEmbeddedNul");
    sp = saved_sp;
}

// === SVALUE_STRING_JOIN Tests ===

TEST_F(StringOperatorsTest, JoinTwoMallocStrings) {
    // Test joining two malloc strings
    lpc::svalue left;
    lpc::svalue right;
    left.set_malloc_string("Left");
    right.set_malloc_string("Right");

    SVALUE_STRING_JOIN(left.raw(), right.raw(), "test");

    assert_string_content(left.raw(), "LeftRight", 9, STRING_MALLOC);
    // right is consumed by the macro; left still owns the joined result.
}

TEST_F(StringOperatorsTest, JoinEmptyStrings) {
    // Test joining two empty strings
    lpc::svalue left;
    lpc::svalue right;
    left.set_malloc_string("");
    right.set_malloc_string("");

    SVALUE_STRING_JOIN(left.raw(), right.raw(), "test");

    assert_string_content(left.raw(), "", 0, STRING_MALLOC);
}

TEST_F(StringOperatorsTest, JoinWithEmbeddedNuls) {
    // Test joining strings with embedded NULs
    const char left_content[] = "A\0B";
    const char right_content[] = "C\0D";

    lpc::svalue left;
    lpc::svalue right;
    left.set_malloc_string(std::string_view(left_content, 3));
    right.set_malloc_string(std::string_view(right_content, 3));

    SVALUE_STRING_JOIN(left.raw(), right.raw(), "test");

    // Result should be "A\0BC\0D" (6 bytes)
    assert_string_content(left.raw(), "A\0BC\0D", 6, STRING_MALLOC);
}

TEST_F(StringOperatorsTest, JoinMallocSelfReuse) {
    // Test joining when left side has ref count 1 (in-place extension)
    lpc::svalue left;
    lpc::svalue right;
    left.set_malloc_string("First");
    right.set_malloc_string("Second");

    SVALUE_STRING_JOIN(left.raw(), right.raw(), "test");

    assert_string_content(left.raw(), "FirstSecond", 11, STRING_MALLOC);
}

// === String Equality Operator Tests ===

TEST_F(StringOperatorsTest, EqualityIdenticalStrings) {
    // Test equality of identical malloc strings via f_eq
    lpc::svalue left;
    lpc::svalue right;
    left.set_malloc_string("Same");
    right.set_malloc_string("Same");

    svalue_t stack[2] = {};
    assign_svalue_no_free(&stack[0], left.raw());
    assign_svalue_no_free(&stack[1], right.raw());
    sp = &stack[1];

    f_eq();

    auto result_view = lpc::svalue_view::from(sp);
    ASSERT_EQ(sp, &stack[0]);
    ASSERT_TRUE(result_view.is_number());
    ASSERT_EQ(result_view.number(), 1);
}

TEST_F(StringOperatorsTest, EqualityDifferentLengths) {
    // Test equality check with length difference detection
    lpc::svalue left;
    lpc::svalue right;
    left.set_malloc_string("Short");
    right.set_malloc_string("Longer");

    ASSERT_TRUE(string_length_differs(left.raw(), right.raw()));
}

TEST_F(StringOperatorsTest, EqualityWithEmbeddedNuls) {
    // Test memcmp equality with embedded NULs
    lpc::svalue s1 ("A\0B"); // should be truncated to "A" (STRING_CONSTANT)
    lpc::svalue s2;
    lpc::svalue s3;
    s2.set_malloc_string(std::string_view("A\0B", 3));
    s3.set_shared_string(std::string_view("A\0B", 3));

    // s1 == s2 == s3 by c_str()
    ASSERT_STREQ(s1.view().c_str(), s2.view().c_str());
    ASSERT_STREQ(s1.view().c_str(), s3.view().c_str());

    // s1 != s2 by length, while s2 == s3 by length and content
    ASSERT_TRUE(string_length_differs(s1.raw(), s2.raw()));
    ASSERT_FALSE(string_length_differs(s2.raw(), s3.raw()));

    svalue_t stack[2] = {};
    auto assert_f_eq = [&](const lpc::svalue &left, const lpc::svalue &right, int64_t expected) {
        free_svalue(&stack[0], "EqualityWithEmbeddedNuls");
        assign_svalue_no_free(&stack[0], left.raw());
        free_svalue(&stack[1], "EqualityWithEmbeddedNuls");
        assign_svalue_no_free(&stack[1], right.raw());
        sp = &stack[1];

        f_eq();

        auto result_view = lpc::svalue_view::from(sp);
        ASSERT_EQ(sp, &stack[0]);
        ASSERT_TRUE(result_view.is_number());
        ASSERT_EQ(result_view.number(), expected);
    };

    assert_f_eq(s1, s2, 0);
    assert_f_eq(s2, s3, 1);
}

// === String Range Operator Tests ===

TEST_F(StringOperatorsTest, RangeFullString) {
    // Test range [0..length-1] returns full string via f_range
    svalue_t stack[3] = {};
    lpc::svalue_view::from(&stack[0]).set_number(0);       // from
    lpc::svalue_view::from(&stack[1]).set_number(6);       // to
    lpc::svalue_view::from(&stack[2]).set_malloc_string("Testing");
    svalue_t *saved_sp = sp;
    sp = &stack[2];

    f_range(0x00);  // NN_RANGE: both numbers normal order

    // f_range pops 3 operands (decrements sp twice), result is at sp
    ASSERT_EQ(sp, &stack[0]);
    auto result_view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(result_view.is_string());
    ASSERT_EQ(result_view.length(), 7u);
    ASSERT_EQ(memcmp(result_view.c_str(), "Testing", 7), 0);

    free_svalue(sp, "RangeFullString");
    sp = saved_sp;
}

TEST_F(StringOperatorsTest, RangeMiddleSlice) {
    // Test range extraction of middle portion via f_range
    svalue_t stack[3] = {};
    lpc::svalue_view::from(&stack[0]).set_number(2);       // from
    lpc::svalue_view::from(&stack[1]).set_number(5);       // to
    lpc::svalue_view::from(&stack[2]).set_malloc_string("0123456789");
    svalue_t *saved_sp = sp;
    sp = &stack[2];

    f_range(0x00);  // NN_RANGE

    ASSERT_EQ(sp, &stack[0]);
    auto result_view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(result_view.is_string());
    ASSERT_EQ(result_view.length(), 4u);
    ASSERT_EQ(memcmp(result_view.c_str(), "2345", 4), 0);

    free_svalue(sp, "RangeMiddleSlice");
    sp = saved_sp;
}

TEST_F(StringOperatorsTest, RangeSingleChar) {
    // Test range extraction of single character via f_range
    svalue_t stack[3] = {};
    lpc::svalue_view::from(&stack[0]).set_number(2);       // from (character 'C')
    lpc::svalue_view::from(&stack[1]).set_number(2);       // to (same as from)
    lpc::svalue_view::from(&stack[2]).set_malloc_string("ABCDE");
    svalue_t *saved_sp = sp;
    sp = &stack[2];

    f_range(0x00);  // NN_RANGE

    ASSERT_EQ(sp, &stack[0]);
    auto result_view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(result_view.is_string());
    ASSERT_EQ(result_view.length(), 1u);
    ASSERT_EQ(result_view.c_str()[0], 'C');

    free_svalue(sp, "RangeSingleChar");
    sp = saved_sp;
}

TEST_F(StringOperatorsTest, RangeWithEmbeddedNul) {
    // Test range extraction that includes embedded NUL via f_range
    const char content[] = "A\0B\0C";
    svalue_t stack[3] = {};
    lpc::svalue_view::from(&stack[0]).set_number(1);       // from
    lpc::svalue_view::from(&stack[1]).set_number(3);       // to
    lpc::svalue_view::from(&stack[2]).set_malloc_string(std::string_view(content, 5));
    svalue_t *saved_sp = sp;
    sp = &stack[2];

    f_range(0x00);  // NN_RANGE

    ASSERT_EQ(sp, &stack[0]);
    auto result_view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(result_view.is_string());
    ASSERT_EQ(result_view.length(), 3u);
    // Result should be "\0B\0" (3 bytes)
    ASSERT_EQ(result_view.c_str()[0], '\0');
    ASSERT_EQ(result_view.c_str()[1], 'B');
    ASSERT_EQ(result_view.c_str()[2], '\0');

    free_svalue(sp, "RangeWithEmbeddedNul");
    sp = saved_sp;
}

// === EXTEND_SVALUE_STRING_LEN Edge Cases and Boundary Conditions ===

TEST_F(StringOperatorsTest, VeryLongStringExtend) {
    // Test extending a large string (tests blkend tracking for USHRT_MAX)
    lpc::svalue target;
    size_t large_size = 100000;
    malloc_str_t large_str = new_string(large_size, "test");
    memset(large_str, 'A', large_size);
    target.set_malloc_string(large_str);
    EXPECT_EQ(target.view().length(), large_size);

    const char *suffix = "END";
    EXTEND_SVALUE_STRING_LEN(target.raw(), suffix, 3, "test");

    size_t final_len = SVALUE_STRLEN(target.raw());
    ASSERT_EQ(final_len, large_size + 3);
    ASSERT_EQ(target.view().length(), final_len);
    ASSERT_EQ(target.view().c_str()[large_size + 2], 'D');
}

TEST_F(StringOperatorsTest, StringLengthConsistency) {
    // Verify SVALUE_STRLEN returns consistent length after operations
    lpc::svalue sv;
    sv.set_constant_string("Initial");
    size_t len1 = SVALUE_STRLEN(sv.raw());
    ASSERT_EQ(sv.view().length(), len1);

    EXTEND_SVALUE_STRING_LEN(sv.raw(), " More", 5, "test");
    EXPECT_TRUE(sv.view().is_malloc());  // Should have converted to malloc string
    size_t len2 = SVALUE_STRLEN(sv.raw());
    ASSERT_EQ(sv.view().length(), len2);

    ASSERT_EQ(len1, 7);
    ASSERT_EQ(len2, 12);
}

TEST_F(StringOperatorsTest, MallocStringLengthConsistency) {
    // Verify malloc string length tracking after extension
    lpc::svalue sv;
    sv.set_malloc_string("Start");

    // Verify initial length
    ASSERT_EQ(SVALUE_STRLEN(sv.raw()), 5);
    
    EXTEND_SVALUE_STRING_LEN(sv.raw(), "End", 3, "test");
    
    // Verify new length is correct  
    ASSERT_EQ(SVALUE_STRLEN(sv.raw()), 8);
}
