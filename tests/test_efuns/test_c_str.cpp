#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

TEST_F(EfunsTest, cStrPreservesCStringInput) {
    push_constant_string("hello");
    f_c_str();

    auto view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(view.is_string());
    EXPECT_STREQ(view.c_str(), "hello");
    EXPECT_EQ(view.length(), 5u);
}

TEST_F(EfunsTest, cStrTruncatesAtFirstEmbeddedNull) {
    lpc::svalue input;
    auto input_view = input.view();
    input_view.set_malloc_string(new_string(11, "test_c_str"));
    memcpy(input_view.malloc_string(), "hello\0world", 11);
    input_view.malloc_string()[11] = '\0';

    push_svalue(input.raw());
    f_c_str();

    auto view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(view.is_string());
    EXPECT_EQ(view.length(), 5u);
    EXPECT_STREQ(view.c_str(), "hello");
}

TEST_F(EfunsTest, cStrLeadingEmbeddedNullBecomesEmptyString) {
    lpc::svalue input;
    auto input_view = input.view();
    input_view.set_malloc_string(new_string(3, "test_c_str"));
    memcpy(input_view.malloc_string(), "\0ab", 3);
    input_view.malloc_string()[3] = '\0';

    push_svalue(input.raw());
    f_c_str();

    auto view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(view.is_string());
    EXPECT_EQ(view.length(), 0u);
    EXPECT_STREQ(view.c_str(), "");
}