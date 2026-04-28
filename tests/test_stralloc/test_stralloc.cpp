#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include "std.h"
#include <gtest/gtest.h>
#include <array>
using namespace testing;

class StrAllocTest: public Test {
protected:
    void SetUp() override {
        // Code here will be called immediately after the constructor (right
        // before each test).
        debug_set_log_with_date (false);
        setlocale(LC_ALL, PLATFORM_UTF8_LOCALE); // force UTF-8 locale for consistent string handling
        init_stem(3, 0177, ""); // use highest debug level and enable all trace logs
        MAIN_OPTION(pedantic) = true; // enable pedantic mode for stricter checks
        debug_message("[ SETUP    ] CTEST_FULL_OUTPUT");
        init_strings (15000, 1000000);   // will be forced to 16384 inside the function
    }
    void TearDown() override {
        // Code here will be called immediately after each test (right
        // before the destructor).
        deinit_strings();
    }
};

TEST_F(StrAllocTest, initialState) {
    EXPECT_EQ(num_distinct_strings, 0);
    EXPECT_EQ(bytes_distinct_strings, 0);
    EXPECT_EQ(overhead_bytes, sizeof(block_t*) * 16384); // 15000 rounded up to next power of two
}

TEST_F(StrAllocTest, makeSharedString) {
    // create shared string (reference counted)
    shared_str_t str1 = make_shared_string("hello world", NULL);
    shared_str_t str2 = make_shared_string("hello world", NULL);
    EXPECT_EQ(str1, str2);

    free_string(to_shared_str(str1));
    shared_str_t str3 = make_shared_string("hello world", NULL);
    EXPECT_EQ(str2, str3);

    free_string(to_shared_str(str2));
    free_string(to_shared_str(str3));
    EXPECT_EQ(num_distinct_strings, 0);
}

TEST_F(StrAllocTest, findString) {
    shared_str_t str1 = make_shared_string("test string", NULL);
    shared_str_t found1 = findstring("test string", NULL); // no reference count increase
    EXPECT_EQ(str1, found1);

    free_string(to_shared_str(str1));
    shared_str_t found2 = findstring("test string", NULL);
    EXPECT_EQ(found2, nullptr); // should not be found after free

    shared_str_t str2 = make_shared_string("test string", NULL);
    found1 = findstring("test string", NULL);
    EXPECT_EQ(str2, found1);
    shared_str_t str3 = ref_string(to_shared_str(found1)); // increase reference count

    free_string(to_shared_str(str2));
    shared_str_t found3 = findstring("test string", NULL);
    EXPECT_EQ(str3, found3); // should still be found due to str3

    free_string(to_shared_str(str3));
    found2 = findstring("test string", NULL);
    EXPECT_EQ(found2, nullptr); // should not be found after all frees
}

TEST_F(StrAllocTest, sharedStringOversizeIsTruncatedAndDeduped) {
    const size_t input_len = static_cast<size_t>(USHRT_MAX) + 123;
    std::string input(input_len, 'x');
    std::string truncated(static_cast<size_t>(USHRT_MAX) - 1, 'x');

    shared_str_t s1 = make_shared_string(input.c_str(), NULL);
    shared_str_t s2 = make_shared_string(input.c_str(), NULL);

    EXPECT_EQ(s1, s2);
    EXPECT_EQ(static_cast<size_t>(USHRT_MAX) - 1, strlen(s1));
    EXPECT_EQ(MSTR_SIZE(s1), static_cast<unsigned short>(USHRT_MAX - 1));
    EXPECT_EQ(findstring(input.c_str(), NULL), nullptr);
    EXPECT_EQ(findstring(truncated.c_str(), NULL), s1);

    free_string(to_shared_str(s1));
    free_string(to_shared_str(s2));
}

TEST_F(StrAllocTest, mallocLongStringTracksBlkendAndCountedLength) {
    const size_t len = static_cast<size_t>(USHRT_MAX) + 37;
    malloc_str_t s = new_string(len, "test");

    memset(s, 'm', len);
    s[len] = '\0';

    EXPECT_EQ(MSTR_SIZE(s), static_cast<unsigned short>(USHRT_MAX));
    EXPECT_EQ(static_cast<char*>(MSTR_BLKEND(s)), s + len);
    EXPECT_EQ(COUNTED_STRLEN(s), len);

    FREE_MSTR(s);
}

TEST_F(StrAllocTest, newStringAlwaysSetsTrailingNulGuard) {
    const size_t len = 7;
    malloc_str_t s = new_string(len, "test");

    EXPECT_EQ(s[len], '\0');

    FREE_MSTR(s);
}

TEST_F(StrAllocTest, extendStringAlwaysSetsTrailingNulGuard) {
    malloc_str_t s = new_string(3, "test");
    memcpy(s, "abc", 3);

    s = extend_string(s, 9);
    EXPECT_EQ(s[9], '\0');

    s = extend_string(s, 2);
    EXPECT_EQ(s[2], '\0');

    FREE_MSTR(s);
}

TEST_F(StrAllocTest, extendStringUpdatesBlkendAcrossThresholds) {
    const size_t short_len = 64;
    const size_t long_len = static_cast<size_t>(USHRT_MAX) + 19;

    malloc_str_t s = new_string(short_len, "test");
    memset(s, 'a', short_len);
    s[short_len] = '\0';

    EXPECT_EQ(MSTR_SIZE(s), static_cast<unsigned short>(short_len));
    EXPECT_EQ(MSTR_BLKEND(s), nullptr);

    s = extend_string(s, long_len);
    memset(s + short_len, 'b', long_len - short_len);
    s[long_len] = '\0';

    EXPECT_EQ(MSTR_SIZE(s), static_cast<unsigned short>(USHRT_MAX));
    EXPECT_EQ(static_cast<char*>(MSTR_BLKEND(s)), s + long_len);
    EXPECT_EQ(COUNTED_STRLEN(s), long_len);

    s = extend_string(s, short_len);
    s[short_len] = '\0';

    EXPECT_EQ(MSTR_SIZE(s), static_cast<unsigned short>(short_len));
    EXPECT_EQ(MSTR_BLKEND(s), nullptr);
    EXPECT_EQ(COUNTED_STRLEN(s), short_len);

    FREE_MSTR(s);
}

TEST_F(StrAllocTest, unlinkLongMallocStringPreservesBlkendLength) {
    const size_t len = static_cast<size_t>(USHRT_MAX) + 53;
    malloc_str_t raw = new_string(len, "test");
    memset(raw, 'u', len);
    raw[len] = '\0';

    MSTR_REF(raw) = 2;

    svalue_t sv;
    lpc::svalue_view sv_view = lpc::svalue_view::from(&sv);
    sv_view.set_malloc_string(raw);

    unlink_string_svalue(&sv);

    EXPECT_NE(sv_view.malloc_string(), raw);
    EXPECT_EQ(MSTR_REF(raw), 1);
    EXPECT_EQ(MSTR_SIZE(sv_view.malloc_string()), static_cast<unsigned short>(USHRT_MAX));
    EXPECT_EQ(static_cast<char*>(MSTR_BLKEND(sv_view.malloc_string())), sv_view.malloc_string() + len);
    EXPECT_EQ(sv_view.length(), len);

    FREE_MSTR(raw);
    FREE_MSTR(sv_view.malloc_string());
}

TEST_F(StrAllocTest, svalueViewTypedStringSettersPreserveSubtypeAndPayload) {
    lpc::svalue shared_owner;
    lpc::svalue malloc_owner;

    shared_str_t shared = make_shared_string("wrapped shared", NULL);
    EXPECT_EQ(COUNTED_REF(shared), 1);

    auto shared_view = shared_owner.view();
    shared_view.set_shared_string(shared);
    ASSERT_TRUE(shared_view.is_string() && shared_view.is_shared());
    EXPECT_EQ(shared_view.shared_string(), shared);
    EXPECT_STREQ(shared_view.c_str(), "wrapped shared");
    EXPECT_EQ(COUNTED_REF(shared), 1);

    malloc_str_t malloced = new_string(5, "test");
    memcpy(malloced, "hello", 5);
    malloced[5] = '\0';

    auto malloc_view = malloc_owner.view();
    malloc_view.set_malloc_string(malloced);
    ASSERT_TRUE(malloc_view.is_string() && malloc_view.is_malloc());
    EXPECT_EQ(malloc_view.malloc_string(), malloced);
    EXPECT_STREQ(malloc_view.c_str(), "hello");
}

TEST_F(StrAllocTest, svalueCopyRetainsSharedStringOwnership) {
    shared_str_t shared = make_shared_string("copy-owned", NULL);

    {
        lpc::svalue owner;
        owner.view().set_shared_string(shared);
        EXPECT_EQ(COUNTED_REF(shared), 1);

        lpc::svalue copy(owner);
        EXPECT_EQ(COUNTED_REF(shared), 2);
        ASSERT_TRUE(copy.view().is_string() && copy.view().is_shared());
        EXPECT_EQ(copy.view().shared_string(), shared);
        EXPECT_STREQ(copy.view().c_str(), "copy-owned");
    }

    EXPECT_EQ(findstring("copy-owned", NULL), nullptr);
}

TEST_F(StrAllocTest, svalueMoveTransfersMallocStringOwnership) {
    malloc_str_t malloced = new_string(4, "test");
    memcpy(malloced, "move", 4);
    malloced[4] = '\0';

    lpc::svalue source;
    source.view().set_malloc_string(malloced);

    lpc::svalue moved(std::move(source));

    EXPECT_TRUE(source.view().is_number());
    EXPECT_EQ(source.view().number(), 0);
    ASSERT_TRUE(moved.view().is_string() && moved.view().is_malloc());
    EXPECT_EQ(moved.view().malloc_string(), malloced);
    EXPECT_STREQ(moved.view().c_str(), "move");
}

TEST_F(StrAllocTest, svalueCopyAssignmentRetainsSharedStringOwnership) {
    shared_str_t shared = make_shared_string("copy-assigned", NULL);
    shared_str_t previous = make_shared_string("old-shared", NULL);

    {
        lpc::svalue source;
        lpc::svalue target;

        source.view().set_shared_string(shared);
        target.view().set_shared_string(previous);

        EXPECT_EQ(COUNTED_REF(shared), 1);
        EXPECT_EQ(COUNTED_REF(previous), 1);

        target = source;

        EXPECT_EQ(COUNTED_REF(shared), 2);
        EXPECT_EQ(findstring("old-shared", NULL), nullptr);
        ASSERT_TRUE(target.view().is_string() && target.view().is_shared());
        EXPECT_EQ(target.view().shared_string(), shared);
        EXPECT_STREQ(target.view().c_str(), "copy-assigned");
    }

    EXPECT_EQ(findstring("copy-assigned", NULL), nullptr);
}

TEST_F(StrAllocTest, svalueMoveAssignmentTransfersMallocStringOwnership) {
    malloc_str_t malloced = new_string(6, "test");
    memcpy(malloced, "assign", 6);
    malloced[6] = '\0';

    shared_str_t previous = make_shared_string("shared-target", NULL);

    lpc::svalue source;
    lpc::svalue target;

    source.view().set_malloc_string(malloced);
    target.view().set_shared_string(previous);

    EXPECT_EQ(COUNTED_REF(previous), 1);

    target = std::move(source);

    EXPECT_EQ(findstring("shared-target", NULL), nullptr);
    EXPECT_TRUE(source.view().is_number());
    EXPECT_EQ(source.view().number(), 0);
    ASSERT_TRUE(target.view().is_string() && target.view().is_malloc());
    EXPECT_EQ(target.view().malloc_string(), malloced);
    EXPECT_STREQ(target.view().c_str(), "assign");
}

TEST_F(StrAllocTest, svalueSelfAssignmentIsNoOp) {
    shared_str_t shared = make_shared_string("self-copy", NULL);
    malloc_str_t malloced = new_string(4, "test");
    memcpy(malloced, "self", 4);
    malloced[4] = '\0';

    {
        lpc::svalue shared_owner;
        shared_owner.view().set_shared_string(shared);

        EXPECT_EQ(COUNTED_REF(shared), 1);

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign"
#endif
        shared_owner = shared_owner;
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

        EXPECT_EQ(COUNTED_REF(shared), 1);
        ASSERT_TRUE(shared_owner.view().is_string() && shared_owner.view().is_shared());
        EXPECT_EQ(shared_owner.view().shared_string(), shared);
        EXPECT_STREQ(shared_owner.view().c_str(), "self-copy");
    }

    EXPECT_EQ(findstring("self-copy", NULL), nullptr);

    {
        lpc::svalue malloc_owner;
        malloc_owner.view().set_malloc_string(malloced);

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
// Suppress self-move warning for this test; we're intentionally testing that self-move is a no-op and doesn't free the malloc string.
#pragma GCC diagnostic ignored "-Wself-move"
#endif
        malloc_owner = std::move(malloc_owner);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

        ASSERT_TRUE(malloc_owner.view().is_string() && malloc_owner.view().is_malloc());
        EXPECT_EQ(malloc_owner.view().malloc_string(), malloced);
        EXPECT_STREQ(malloc_owner.view().c_str(), "self");
    }
}

TEST_F(StrAllocTest, svalueRefRetainsSharedStringPayload) {
    shared_str_t shared = make_shared_string("borrowed-shared", NULL);
    svalue_t raw{};
    lpc::svalue_view::from(&raw).set_shared_string(shared);

    EXPECT_EQ(COUNTED_REF(shared), 1);

    {
        lpc::svalue_ref ref(&raw);

        EXPECT_EQ(COUNTED_REF(shared), 2);
        ASSERT_TRUE(ref.view().is_string());
        ASSERT_TRUE(ref.view().is_shared());
        EXPECT_EQ(ref.view().shared_string(), shared);
        EXPECT_STREQ(ref.view().c_str(), "borrowed-shared");

        lpc::svalue_ref copied(ref);
        EXPECT_EQ(COUNTED_REF(shared), 3);
        ASSERT_TRUE(copied.view().is_shared());
        EXPECT_EQ(copied.view().shared_string(), shared);
    }

    EXPECT_EQ(COUNTED_REF(shared), 1);
    free_string_svalue(&raw);
    EXPECT_EQ(findstring("borrowed-shared", NULL), nullptr);
}

TEST_F(StrAllocTest, svalueRefRetainsMallocStringPayload) {
    malloc_str_t malloced = new_string(6, "test");
    memcpy(malloced, "borrow", 6);
    malloced[6] = '\0';

    svalue_t raw{};
    lpc::svalue_view::from(&raw).set_malloc_string(malloced);

    EXPECT_EQ(COUNTED_REF(malloced), 1);

    {
        lpc::svalue_ref ref(&raw);

        EXPECT_EQ(COUNTED_REF(malloced), 2);
        ASSERT_TRUE(ref.view().is_string());
        ASSERT_TRUE(ref.view().is_malloc());
        EXPECT_EQ(ref.view().malloc_string(), malloced);
        EXPECT_STREQ(ref.view().c_str(), "borrow");

        lpc::svalue_ref moved(std::move(ref));
        EXPECT_EQ(COUNTED_REF(malloced), 2);
        EXPECT_FALSE(ref);
        ASSERT_TRUE(moved.view().is_malloc());
        EXPECT_EQ(moved.view().malloc_string(), malloced);
    }

    EXPECT_EQ(COUNTED_REF(malloced), 1);
    free_string_svalue(&raw);
}

TEST_F(StrAllocTest, svalueViewApiExposesTypedOperations) {
    lpc::svalue value;

    value.view().set_number(42);
    EXPECT_TRUE(value.view().is_number());
    EXPECT_EQ(value.view().number(), 42);

    value.view().set_constant_string("alpha");
    ASSERT_TRUE(value.view().is_string());
    EXPECT_STREQ(value.view().c_str(), "alpha");
}


TEST_F(StrAllocTest, sharedStringAtUshortMaxIsTruncatedToUshortMaxMinusOne) {
    const size_t input_len = static_cast<size_t>(USHRT_MAX);
    std::string input(input_len, 'z');
    std::string truncated(static_cast<size_t>(USHRT_MAX) - 1, 'z');

    shared_str_t s = make_shared_string(input.c_str(), NULL);

    EXPECT_EQ(strlen(s), static_cast<size_t>(USHRT_MAX) - 1);
    EXPECT_EQ(MSTR_SIZE(s), static_cast<unsigned short>(USHRT_MAX - 1));
    EXPECT_EQ(findstring(truncated.c_str(), NULL), s);
    EXPECT_EQ(findstring(input.c_str(), NULL), nullptr);

    free_string(to_shared_str(s));
}

TEST_F(StrAllocTest, sharedStringFromNonNullTerminatedSpanRoundTrips) {
    std::array<char, 5> bytes = {'h', 'e', 'l', 'l', 'o'};

    shared_str_t s = make_shared_string(bytes.data(), bytes.data() + bytes.size());
    EXPECT_NE(s, nullptr);
    EXPECT_EQ(MSTR_SIZE(s), bytes.size());

    shared_str_t found_span = findstring(bytes.data(), bytes.data() + bytes.size());
    EXPECT_EQ(found_span, s);

    shared_str_t found_cstr = findstring("hello", NULL);
    EXPECT_EQ(found_cstr, s);

    free_string(to_shared_str(s));
}

TEST_F(StrAllocTest, sharedStringEmbeddedNulSpanIsDistinctFromPrefix) {
    std::array<char, 5> bytes = {'a', 'b', '\0', 'c', 'd'};

    shared_str_t s1 = make_shared_string(bytes.data(), bytes.data() + bytes.size());
    shared_str_t s2 = make_shared_string(bytes.data(), bytes.data() + bytes.size());

    EXPECT_EQ(s1, s2);
    EXPECT_EQ(MSTR_SIZE(s1), bytes.size());
    EXPECT_EQ(memcmp(s1, bytes.data(), bytes.size()), 0);

    shared_str_t found_span = findstring(bytes.data(), bytes.data() + bytes.size());
    EXPECT_EQ(found_span, s1);

    shared_str_t found_prefix = findstring("ab", NULL);
    EXPECT_EQ(found_prefix, nullptr);

    free_string(to_shared_str(s1));
    free_string(to_shared_str(s2));
}
