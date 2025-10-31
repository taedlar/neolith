#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

using namespace testing;

TEST_F(EfunsTest, stringCaseConversion) {
    push_constant_string("Hello World!");

    f_upper_case();
    ASSERT_EQ(sp->type, T_STRING);
    ASSERT_STREQ(sp->u.string, "HELLO WORLD!");

    f_lower_case();
    ASSERT_EQ(sp->type, T_STRING);
    ASSERT_STREQ(sp->u.string, "hello world!");

    f_capitalize();
    ASSERT_EQ(sp->type, T_STRING);
    ASSERT_STREQ(sp->u.string, "Hello world!");
}
