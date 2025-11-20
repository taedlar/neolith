#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

using namespace testing;

TEST_F(StackMachineTest, initialState) {
    // =================================================
    // stack machine initial state: see reset_machine()
    // =================================================
    ASSERT_EQ(get_machine_state(), MS_PRE_MUDLIB);
    ASSERT_EQ(obj_list, nullptr); // no objects loaded yet
    ASSERT_EQ(obj_list_destruct, nullptr); // no objects to destruct
    EXPECT_TRUE(csp == control_stack - 1); // control stack pointer at the bottom
    EXPECT_TRUE(sp != nullptr); // stack pointer initialized
}

TEST_F(StackMachineTest, pushValueStatic) {
    ASSERT_TRUE(sp != nullptr); // stack pointer initialized
    svalue_t *initial_sp = sp;

    push_number(42);
    ASSERT_EQ(sp->type, T_NUMBER);
    ASSERT_EQ(sp->u.number, 42);

    push_real(3.14);
    ASSERT_EQ(sp->type, T_REAL);
    ASSERT_DOUBLE_EQ(sp->u.real, 3.14);

    push_undefined();
    ASSERT_EQ(sp->type, T_NUMBER);
    ASSERT_EQ(sp->subtype, T_UNDEFINED);

    push_constant_string("hello");
    ASSERT_EQ(sp->type, T_STRING);
    ASSERT_STREQ(sp->u.const_string, "hello");
    ASSERT_EQ(sp->subtype, STRING_CONSTANT);

    // Stack pointer should have moved up by 4 svalue_ts
    ASSERT_EQ(sp, initial_sp + 4);

    pop_n_elems(4);
    ASSERT_EQ(sp, initial_sp); // back to initial position
}

TEST_F(StackMachineTest, pushValueAlloc) {
    ASSERT_TRUE(sp != nullptr); // stack pointer initialized
    svalue_t *initial_sp = sp;

    copy_and_push_string("dynamic string");
    ASSERT_EQ(sp->type, T_STRING);
    ASSERT_STREQ(sp->u.string, "dynamic string");
    ASSERT_EQ(sp->subtype, STRING_MALLOC);
    EXPECT_FALSE(findstring("dynamic string")); // a private malloced string, not shared

    share_and_push_string("shared string");
    ASSERT_EQ(sp->type, T_STRING);
    ASSERT_STREQ(sp->u.string, "shared string");
    ASSERT_EQ(sp->subtype, STRING_SHARED);
    EXPECT_TRUE(findstring("shared string")); // a shared string

    share_and_push_string("shared string");
    ASSERT_EQ(sp->type, T_STRING);
    ASSERT_EQ(sp->u.string, (sp - 1)->u.string); // pointer should be the same, reference counted
    ASSERT_EQ(sp->subtype, STRING_SHARED);

    pop_n_elems(3);
    ASSERT_EQ(sp, initial_sp); // back to initial position
}
