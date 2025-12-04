#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
extern "C" {
    #include "std.h"
    #include "stralloc.h"
}
#include <gtest/gtest.h>
using namespace testing;

class StrAllocTest: public Test {
protected:
    void SetUp() override {
        // Code here will be called immediately after the constructor (right
        // before each test).
        debug_set_log_with_date (0);
        setlocale(LC_ALL, PLATFORM_UTF8_LOCALE); // force UTF-8 locale for consistent string handling
        init_stem(3, 0177, ""); // use highest debug level and enable all trace logs
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
    EXPECT_EQ(overhead_bytes, sizeof(block_t*) * 16384);
}

TEST_F(StrAllocTest, makeSharedString) {
    // create shared string (reference counted)
    char* str1 = make_shared_string("hello world");
    char* str2 = make_shared_string("hello world");
    EXPECT_EQ(str1, str2);

    free_string(str1);
    char* str3 = make_shared_string("hello world");
    EXPECT_EQ(str2, str3);

    free_string(str2);
    free_string(str3);
    EXPECT_EQ(num_distinct_strings, 0);
}

TEST_F(StrAllocTest, findString) {
    char* str1 = make_shared_string("test string");
    char* found1 = findstring("test string"); // no reference count increase
    EXPECT_EQ(str1, found1);

    free_string(str1);
    char* found2 = findstring("test string");
    EXPECT_EQ(found2, nullptr); // should not be found after free

    char* str2 = make_shared_string("test string");
    found1 = findstring("test string");
    EXPECT_EQ(str2, found1);
    char* str3 = ref_string(found1); // increase reference count

    free_string(str2);
    char* found3 = findstring("test string");
    EXPECT_EQ(str3, found3); // should still be found due to str3

    free_string(str3);
    found2 = findstring("test string");
    EXPECT_EQ(found2, nullptr); // should not be found after all frees
}
