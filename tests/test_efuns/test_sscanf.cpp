#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"
#include "lpc/types.hpp"

extern "C" {
    #include "efuns/sscanf.h"
}

TEST_F(EfunsTest, sscanfBasic) {
    svalue_t *framep = sp + 1; // frame pointer

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

    ASSERT_TRUE(lpc::svalue_view::from(framep).is_number());
    ASSERT_EQ(lpc::svalue_view::from(framep).number(), 3); // number of assignments

    svalue_t *arg = framep + 1;
    ASSERT_TRUE(lpc::svalue_view::from(arg).is_real());
    ASSERT_DOUBLE_EQ(lpc::svalue_view::from(arg).real(), 45.67);

    arg++;
    ASSERT_TRUE(lpc::svalue_view::from(arg).is_string());
    ASSERT_STREQ(lpc::svalue_view::from(arg).c_str(), "abc");

    arg++;
    ASSERT_TRUE(lpc::svalue_view::from(arg).is_number());
    ASSERT_EQ(lpc::svalue_view::from(arg).number(), 123);
}
