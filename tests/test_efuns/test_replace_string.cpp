#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

TEST_F(EfunsTest, replaceStringThreeArgs) {
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
        FAIL() << "Error occurred during replace_string efun tests.";
    }
    else {
        // replace "world" with "there" in "hello world"
        copy_and_push_string("hello world");
        push_constant_string("world");
        push_constant_string("there");
        st_num_arg = 3;
        f_replace_string();

        ASSERT_EQ(sp->type, T_STRING);
        EXPECT_STREQ(sp->u.string, "hello there");

        // replace "大" with "小" in "大千世界"
        copy_and_push_string("大千世界");
        push_constant_string("大");
        push_constant_string("小");
        st_num_arg = 3;
        f_replace_string();

        ASSERT_EQ(sp->type, T_STRING);
        EXPECT_STREQ(sp->u.string, "小千世界");
    }
}

TEST_F(EfunsTest, replaceStringFourArgs) {
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
        FAIL() << "Error occurred during replace_string efun tests.";
    }
    else {
        // replace "world" with "there" in "hello world"
        copy_and_push_string("to be or not to be");
        push_constant_string("be");
        push_constant_string("do");
        push_number(0); // replace all occurrences
        st_num_arg = 4;
        f_replace_string();

        ASSERT_EQ(sp->type, T_STRING);
        EXPECT_STREQ(sp->u.string, "to do or not to do");

        push_constant_string("do");
        push_constant_string("be");
        push_number(1); // first occurrence
        st_num_arg = 4;
        f_replace_string();

        ASSERT_EQ(sp->type, T_STRING);
        EXPECT_STREQ(sp->u.string, "to be or not to do");
    }
}

TEST_F(EfunsTest, replaceStringFiveArgs) {
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
        FAIL() << "Error occurred during replace_string efun tests.";
    }
    else {
        // replace "world" with "there" in "hello world"
        copy_and_push_string("to be or not to be");
        push_constant_string("be");
        push_constant_string("do");
        push_number(2); // second occurrence
        push_number(2); // up to second occurrence
        st_num_arg = 5;
        f_replace_string();

        ASSERT_EQ(sp->type, T_STRING);
        EXPECT_STREQ(sp->u.string, "to be or not to do");
    }
}
