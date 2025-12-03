#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

extern "C" {
    #include "efuns/sscanf.h"
}

using namespace testing;

TEST_F(EfunsTest, sscanfBasic) {
    svalue_t *fp = sp + 1; // frame pointer

    // mock pc to indicate number of lvalue args
    unsigned char number_of_args = 3;
    pc = (char *)&number_of_args;

    // before calling f_sscanf, stack looks like:
    // [source string][format string] <-- sp
    push_constant_string("123 abc 45.67");
    push_constant_string("%d %s %f");
    f_sscanf();
    // after calling f_sscanf, stack looks like:
    // [number of assignments][arg3][arg2][arg1] <-- sp

    ASSERT_EQ(fp->type, T_NUMBER);
    ASSERT_EQ(fp->u.number, 3); // number of assignments

    svalue_t *arg = fp + 1;
    ASSERT_EQ(arg->type, T_REAL);
    ASSERT_DOUBLE_EQ(arg->u.real, 45.67);

    arg++;
    ASSERT_EQ(arg->type, T_STRING);
    ASSERT_STREQ(arg->u.string, "abc");

    arg++;
    ASSERT_EQ(arg->type, T_NUMBER);
    ASSERT_EQ(arg->u.number, 123);
}
