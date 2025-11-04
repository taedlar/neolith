#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

TEST_F(EfunsTest, strsrchBasic) {
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
        FAIL() << "Error occurred during strsrch efun tests.";
    }
    else {
        // search "world" in "hello world" from left to right
        push_constant_string("hello world");
        push_constant_string("world");
        push_number(0); // left to right
        f_strsrch();

        ASSERT_EQ(sp->type, T_NUMBER);
        ASSERT_EQ(sp->u.number, 6); // "world" starts at index 6

        push_constant_string("大千世界");
        push_number(L'千');
        push_number(0); // left to right
        f_strsrch();
        ASSERT_EQ(sp->type, T_NUMBER);
        ASSERT_EQ(sp->u.number, 3); // '千' starts at byte index 3 in UTF-8
    }
}
