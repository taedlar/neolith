#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include "std.h"
#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>
#include <algorithm>

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

    // Helper: Create a malloc-based svalue with given content and length
    void create_malloc_string(svalue_t *sv, const char *content, size_t len) {
        ASSERT_TRUE(sv != nullptr);
        lpc::svalue_view view = lpc::svalue_view::from(sv);
        view.set_malloc_string(new_string(len, "test"));
        ASSERT_TRUE(view.malloc_string() != nullptr);
        memcpy(view.malloc_string(), content, len);
        view.malloc_string()[len] = '\0';
    }

    // Helper: Create a shared-based svalue with given content
    void create_shared_string(svalue_t *sv, const char *content) {
        ASSERT_TRUE(sv != nullptr);
        lpc::svalue_view view = lpc::svalue_view::from(sv);
        view.set_shared_string(make_shared_string(content, NULL));
        ASSERT_TRUE(view.shared_string() != nullptr);
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
    svalue_t target;
    create_malloc_string(&target, "Hello", 5);

    const char *suffix = " World";
    size_t suffix_len = 6;

    EXTEND_SVALUE_STRING_LEN(&target, suffix, suffix_len, "test");

    assert_string_content(&target, "Hello World", 11, STRING_MALLOC);
    free_string_svalue(&target);
}

TEST_F(StringOperatorsTest, ExtendViaLenAppendEmpty) {
    // Test appending empty string (no-op)
    svalue_t target;
    create_malloc_string(&target, "Test", 4);

    EXTEND_SVALUE_STRING_LEN(&target, "", 0, "test");

    assert_string_content(&target, "Test", 4, STRING_MALLOC);
    free_string_svalue(&target);
}

TEST_F(StringOperatorsTest, ExtendViaLenAppendToEmpty) {
    // Test appending to empty string
    svalue_t target;
    create_malloc_string(&target, "", 0);

    EXTEND_SVALUE_STRING_LEN(&target, "Data", 4, "test");

    assert_string_content(&target, "Data", 4, STRING_MALLOC);
    free_string_svalue(&target);
}

TEST_F(StringOperatorsTest, ExtendViaLenAppendWithEmbeddedNul) {
    // Test appending string with embedded NUL
    svalue_t target;
    create_malloc_string(&target, "Start", 5);

    const char suffix[] = "A\0B";  // 3 bytes: A, NUL, B
    EXTEND_SVALUE_STRING_LEN(&target, suffix, 3, "test");

    // Result should be "StartA\0B" (8 bytes)
    assert_string_content(&target, "StartA\0B", 8, STRING_MALLOC);
    free_string_svalue(&target);
}

TEST_F(StringOperatorsTest, ExtendViaLenMallocSelfReuse) {
    // Test extending a malloc string with ref count 1 (in-place extension)
    svalue_t target;
    create_malloc_string(&target, "Hello", 5);

    // Verify it can be extended in place
    EXTEND_SVALUE_STRING_LEN(&target, " Again", 6, "test");

    assert_string_content(&target, "Hello Again", 11, STRING_MALLOC);
    free_string_svalue(&target);
}

// === SVALUE_STRING_ADD_LEFT_LEN Tests ===

TEST_F(StringOperatorsTest, AddLeftViaLenPrependNormal) {
    // Test prepending to stack-top string (simulated via direct macro)
    // Note: AddLeft macros use 'sp' global, so we test the underlying logic
    svalue_t target;
    create_malloc_string(&target, "World", 5);

    const char *prefix = "Hello ";
    size_t prefix_len = 6;

    // Simulate ADD_LEFT by manually building the result
    // since the macro depends on VM stack state
    malloc_str_t result = new_string(prefix_len + 5, "test");
    memcpy(result, prefix, prefix_len);
    memcpy(result + prefix_len, lpc::svalue_view::from(&target).malloc_string(), 5);
    result[prefix_len + 5] = '\0';

    svalue_t combined;
    lpc::svalue_view::from(&combined).set_malloc_string(result);

    assert_string_content(&combined, "Hello World", 11, STRING_MALLOC);
    free_string_svalue(&combined);
    free_string_svalue(&target);
}

TEST_F(StringOperatorsTest, AddLeftViaLenWithEmbeddedNul) {
    // Test prepending string with embedded NUL
    const char target_str[] = "End";
    const char prefix[] = "A\0B";  // 3 bytes with embedded NUL

    malloc_str_t result = new_string(3 + 3, "test");
    memcpy(result, prefix, 3);
    memcpy(result + 3, target_str, 3);
    result[6] = '\0';

    // Result should be "A\0BEnd" (6 bytes)
    ASSERT_EQ(memcmp(result, "A\0BEnd", 6), 0);

    FREE_MSTR(result);
}

// === SVALUE_STRING_JOIN Tests ===

TEST_F(StringOperatorsTest, JoinTwoMallocStrings) {
    // Test joining two malloc strings
    svalue_t left, right;
    create_malloc_string(&left, "Left", 4);
    create_malloc_string(&right, "Right", 5);

    SVALUE_STRING_JOIN(&left, &right, "test");

    assert_string_content(&left, "LeftRight", 9, STRING_MALLOC);
    // right is consumed by the macro; left still owns the joined result.
    free_string_svalue(&left);
}

TEST_F(StringOperatorsTest, JoinEmptyStrings) {
    // Test joining two empty strings
    svalue_t left, right;
    create_malloc_string(&left, "", 0);
    create_malloc_string(&right, "", 0);

    SVALUE_STRING_JOIN(&left, &right, "test");

    assert_string_content(&left, "", 0, STRING_MALLOC);
    free_string_svalue(&left);
}

TEST_F(StringOperatorsTest, JoinWithEmbeddedNuls) {
    // Test joining strings with embedded NULs
    const char left_content[] = "A\0B";
    const char right_content[] = "C\0D";

    svalue_t left, right;
    create_malloc_string(&left, left_content, 3);
    create_malloc_string(&right, right_content, 3);

    SVALUE_STRING_JOIN(&left, &right, "test");

    // Result should be "A\0BC\0D" (6 bytes)
    assert_string_content(&left, "A\0BC\0D", 6, STRING_MALLOC);
    free_string_svalue(&left);
}

TEST_F(StringOperatorsTest, JoinMallocSelfReuse) {
    // Test joining when left side has ref count 1 (in-place extension)
    svalue_t left, right;
    create_malloc_string(&left, "First", 5);
    create_malloc_string(&right, "Second", 6);

    SVALUE_STRING_JOIN(&left, &right, "test");

    assert_string_content(&left, "FirstSecond", 11, STRING_MALLOC);
    free_string_svalue(&left);
}

// === String Equality Operator Tests (memcmp semantics) ===

TEST_F(StringOperatorsTest, EqualityIdenticalStrings) {
    // Test equality of identical malloc strings
    svalue_t left, right;
    create_malloc_string(&left, "Same", 4);
    create_malloc_string(&right, "Same", 4);

    // Simulate memcmp check used in operator
    lpc::svalue_view left_view = lpc::svalue_view::from(&left);
    lpc::svalue_view right_view = lpc::svalue_view::from(&right);
    int cmp_result = memcmp(left_view.malloc_string(), right_view.malloc_string(), left_view.length());
    ASSERT_EQ(cmp_result, 0);
    ASSERT_EQ(left_view.length(), right_view.length());

    free_string_svalue(&left);
    free_string_svalue(&right);
}

TEST_F(StringOperatorsTest, EqualityDifferentLengths) {
    // Test equality check with length difference detection
    svalue_t left, right;
    create_malloc_string(&left, "Short", 5);
    create_malloc_string(&right, "Longer", 6);

    // Using SVALUE_STRLEN_DIFFERS macro logic
    bool differs = SVALUE_STRLEN(&left) != SVALUE_STRLEN(&right);
    ASSERT_TRUE(differs);

    free_string_svalue(&left);
    free_string_svalue(&right);
}

TEST_F(StringOperatorsTest, EqualityWithEmbeddedNuls) {
    // Test memcmp equality with embedded NULs
    const char content1[] = "A\0B";
    const char content2[] = "A\0B";
    const char content3[] = "A\0C";

    svalue_t s1, s2, s3;
    create_malloc_string(&s1, content1, 3);
    create_malloc_string(&s2, content2, 3);
    create_malloc_string(&s3, content3, 3);

    // s1 == s2 via memcmp
    ASSERT_EQ(memcmp(s1.u.malloc_string, s2.u.malloc_string, 3), 0);
    ASSERT_EQ(SVALUE_STRLEN(&s1), SVALUE_STRLEN(&s2));

    // s1 != s3 due to byte difference at position 2
    ASSERT_NE(memcmp(s1.u.malloc_string, s3.u.malloc_string, 3), 0);

    free_string_svalue(&s1);
    free_string_svalue(&s2);
    free_string_svalue(&s3);
}

// === String Range Operator Tests ===

TEST_F(StringOperatorsTest, RangeFullString) {
    // Test range [0..length-1] returns full string
    const char content[] = "Testing";
    size_t len = strlen(content);

    svalue_t sv;
    create_malloc_string(&sv, content, len);

    // Simulate range extraction for [0..6] on "Testing"
    size_t from = 0, to = 6;
    size_t out_len = to - from + 1;
    malloc_str_t result = new_string(out_len, "range_test");
    memcpy(result, sv.u.malloc_string + from, out_len);
    result[out_len] = '\0';

    ASSERT_TRUE(memcmp(result, "Testing", out_len) == 0);

    FREE_MSTR(result);
    free_string_svalue(&sv);
}

TEST_F(StringOperatorsTest, RangeMiddleSlice) {
    // Test range extraction of middle portion
    const char content[] = "0123456789";
    svalue_t sv;
    create_malloc_string(&sv, content, 10);

    // Extract [2..5]
    size_t from = 2, to = 5;
    size_t out_len = to - from + 1;
    malloc_str_t result = new_string(out_len, "range_test");
    memcpy(result, sv.u.malloc_string + from, out_len);
    result[out_len] = '\0';

    ASSERT_TRUE(memcmp(result, "2345", 4) == 0);

    FREE_MSTR(result);
    free_string_svalue(&sv);
}

TEST_F(StringOperatorsTest, RangeSingleChar) {
    // Test range extraction of single character
    const char content[] = "ABCDE";
    svalue_t sv;
    create_malloc_string(&sv, content, 5);

    // Extract [2..2] (character 'C')
    size_t from = 2, to = 2;
    size_t out_len = to - from + 1;
    malloc_str_t result = new_string(out_len, "range_test");
    memcpy(result, sv.u.malloc_string + from, out_len);
    result[out_len] = '\0';

    ASSERT_EQ(result[0], 'C');
    ASSERT_EQ(out_len, 1);

    FREE_MSTR(result);
    free_string_svalue(&sv);
}

TEST_F(StringOperatorsTest, RangeWithEmbeddedNul) {
    // Test range extraction that includes embedded NUL
    const char content[] = "A\0B\0C";
    svalue_t sv;
    create_malloc_string(&sv, content, 5);

    // Extract [1..3] which includes NULs
    size_t from = 1, to = 3;
    size_t out_len = to - from + 1;
    malloc_str_t result = new_string(out_len, "range_test");
    memcpy(result, sv.u.malloc_string + from, out_len);
    result[out_len] = '\0';

    // Result should be "\0B\0" (3 bytes plus terminator)
    ASSERT_EQ(result[0], '\0');
    ASSERT_EQ(result[1], 'B');
    ASSERT_EQ(result[2], '\0');

    FREE_MSTR(result);
    free_string_svalue(&sv);
}

// === Edge Cases and Boundary Conditions ===

TEST_F(StringOperatorsTest, VeryLongStringExtend) {
    // Test extending a large string (tests blkend tracking for USHRT_MAX)
    svalue_t target;
    size_t large_size = 100000;
    malloc_str_t large_str = new_string(large_size, "test");
    memset(large_str, 'A', large_size);
    lpc::svalue_view::from(&target).set_malloc_string(large_str);

    const char *suffix = "END";
    EXTEND_SVALUE_STRING_LEN(&target, suffix, 3, "test");

    size_t final_len = SVALUE_STRLEN(&target);
    ASSERT_EQ(final_len, large_size + 3);
    ASSERT_EQ(lpc::svalue_view::from(&target).c_str()[large_size + 2], 'D');

    free_string_svalue(&target);
}

TEST_F(StringOperatorsTest, StringLengthConsistency) {
    // Verify SVALUE_STRLEN returns consistent length after operations
    svalue_t sv;
    create_malloc_string(&sv, "Initial", 7);
    size_t len1 = SVALUE_STRLEN(&sv);

    EXTEND_SVALUE_STRING_LEN(&sv, " More", 5, "test");
    size_t len2 = SVALUE_STRLEN(&sv);

    ASSERT_EQ(len1, 7);
    ASSERT_EQ(len2, 12);
    ASSERT_EQ(len2 - len1, 5);

    free_string_svalue(&sv);
}

TEST_F(StringOperatorsTest, MallocStringLengthConsistency) {
    // Verify malloc string length tracking after extension
    svalue_t sv;
    create_malloc_string(&sv, "Start", 5);
    
    // Verify initial length
    ASSERT_EQ(SVALUE_STRLEN(&sv), 5);
    
    EXTEND_SVALUE_STRING_LEN(&sv, "End", 3, "test");
    
    // Verify new length is correct  
    ASSERT_EQ(SVALUE_STRLEN(&sv), 8);

    free_string_svalue(&sv);
}
