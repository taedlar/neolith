#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
extern "C" {
    #include "std.h"
    #include "stralloc.h"
}
#include <gtest/gtest.h>
#include <array>
using namespace testing;

class StrAllocTest: public Test {
protected:
    void SetUp() override {
        // Code here will be called immediately after the constructor (right
        // before each test).
        debug_set_log_with_date (0);
        setlocale(LC_ALL, PLATFORM_UTF8_LOCALE); // force UTF-8 locale for consistent string handling
        init_stem(3, 0177, ""); // use highest debug level and enable all trace logs
        MAIN_OPTION(pedantic) = 1; // enable pedantic mode for stricter checks
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
    char* str1 = make_shared_string("hello world", NULL);
    char* str2 = make_shared_string("hello world", NULL);
    EXPECT_EQ(str1, str2);

    free_string(str1);
    char* str3 = make_shared_string("hello world", NULL);
    EXPECT_EQ(str2, str3);

    free_string(str2);
    free_string(str3);
    EXPECT_EQ(num_distinct_strings, 0);
}

TEST_F(StrAllocTest, findString) {
    char* str1 = make_shared_string("test string", NULL);
    char* found1 = findstring("test string", NULL); // no reference count increase
    EXPECT_EQ(str1, found1);

    free_string(str1);
    char* found2 = findstring("test string", NULL);
    EXPECT_EQ(found2, nullptr); // should not be found after free

    char* str2 = make_shared_string("test string", NULL);
    found1 = findstring("test string", NULL);
    EXPECT_EQ(str2, found1);
    char* str3 = ref_string(found1); // increase reference count

    free_string(str2);
    char* found3 = findstring("test string", NULL);
    EXPECT_EQ(str3, found3); // should still be found due to str3

    free_string(str3);
    found2 = findstring("test string", NULL);
    EXPECT_EQ(found2, nullptr); // should not be found after all frees
}

TEST_F(StrAllocTest, sharedStringOversizeIsTruncatedAndDeduped) {
    const size_t input_len = static_cast<size_t>(USHRT_MAX) + 123;
    std::string input(input_len, 'x');
    std::string truncated(static_cast<size_t>(USHRT_MAX) - 1, 'x');

    char* s1 = make_shared_string(input.c_str(), NULL);
    char* s2 = make_shared_string(input.c_str(), NULL);

    EXPECT_EQ(s1, s2);
    EXPECT_EQ(static_cast<size_t>(USHRT_MAX) - 1, strlen(s1));
    EXPECT_EQ(MSTR_SIZE(s1), static_cast<unsigned short>(USHRT_MAX - 1));
    EXPECT_EQ(findstring(input.c_str(), NULL), nullptr);
    EXPECT_EQ(findstring(truncated.c_str(), NULL), s1);

    free_string(s1);
    free_string(s2);
}

TEST_F(StrAllocTest, mallocLongStringTracksBlkendAndCountedLength) {
    const size_t len = static_cast<size_t>(USHRT_MAX) + 37;
    char* s = new_string(len, "test");

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
    sv.type = T_STRING;
    sv.subtype = STRING_MALLOC;
    sv.u.malloc_string = raw;

    unlink_string_svalue(&sv);

    EXPECT_NE(sv.u.malloc_string, raw);
    EXPECT_EQ(MSTR_REF(raw), 1);
    EXPECT_EQ(MSTR_SIZE(sv.u.malloc_string), static_cast<unsigned short>(USHRT_MAX));
    EXPECT_EQ(static_cast<char*>(MSTR_BLKEND(sv.u.malloc_string)), sv.u.malloc_string + len);
    EXPECT_EQ(COUNTED_STRLEN(sv.u.malloc_string), len);

    FREE_MSTR(raw);
    FREE_MSTR(sv.u.malloc_string);
}

TEST_F(StrAllocTest, longMallocStringFallbackWorksWhenBlkendMissing) {
    const size_t len = static_cast<size_t>(USHRT_MAX) + 29;
    char* s = new_string(len, "test");
    memset(s, 'f', len);
    s[len] = '\0';

    MSTR_BLKEND(s) = nullptr;

    EXPECT_EQ(MSTR_SIZE(s), static_cast<unsigned short>(USHRT_MAX));
    EXPECT_EQ(COUNTED_STRLEN(s), len);

    FREE_MSTR(s);
}

TEST_F(StrAllocTest, sharedStringAtUshortMaxIsTruncatedToUshortMaxMinusOne) {
    const size_t input_len = static_cast<size_t>(USHRT_MAX);
    std::string input(input_len, 'z');
    std::string truncated(static_cast<size_t>(USHRT_MAX) - 1, 'z');

    char* s = make_shared_string(input.c_str(), NULL);

    EXPECT_EQ(strlen(s), static_cast<size_t>(USHRT_MAX) - 1);
    EXPECT_EQ(MSTR_SIZE(s), static_cast<unsigned short>(USHRT_MAX - 1));
    EXPECT_EQ(findstring(truncated.c_str(), NULL), s);
    EXPECT_EQ(findstring(input.c_str(), NULL), nullptr);

    free_string(s);
}

TEST_F(StrAllocTest, sharedStringFromNonNullTerminatedSpanRoundTrips) {
    std::array<char, 5> bytes = {'h', 'e', 'l', 'l', 'o'};

    char* s = make_shared_string(bytes.data(), bytes.data() + bytes.size());
    EXPECT_NE(s, nullptr);
    EXPECT_EQ(MSTR_SIZE(s), bytes.size());

    char* found_span = findstring(bytes.data(), bytes.data() + bytes.size());
    EXPECT_EQ(found_span, s);

    char* found_cstr = findstring("hello", NULL);
    EXPECT_EQ(found_cstr, s);

    free_string(s);
}

TEST_F(StrAllocTest, sharedStringEmbeddedNulSpanIsDistinctFromPrefix) {
    std::array<char, 5> bytes = {'a', 'b', '\0', 'c', 'd'};

    char* s1 = make_shared_string(bytes.data(), bytes.data() + bytes.size());
    char* s2 = make_shared_string(bytes.data(), bytes.data() + bytes.size());

    EXPECT_EQ(s1, s2);
    EXPECT_EQ(MSTR_SIZE(s1), bytes.size());
    EXPECT_EQ(memcmp(s1, bytes.data(), bytes.size()), 0);

    char* found_span = findstring(bytes.data(), bytes.data() + bytes.size());
    EXPECT_EQ(found_span, s1);

    char* found_prefix = findstring("ab", NULL);
    EXPECT_EQ(found_prefix, nullptr);

    free_string(s1);
    free_string(s2);
}
