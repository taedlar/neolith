#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

using namespace testing;

TEST_F(StackMachineTest, initialState) {
    // =================================================
    // stack machine initial state: see reset_interpreter()
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
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_number());
    ASSERT_EQ(lpc::svalue_view::from(sp).number(), 42);

    push_real(3.14);
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_real());
    ASSERT_DOUBLE_EQ(lpc::svalue_view::from(sp).real(), 3.14);

    push_undefined();
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_number());
    ASSERT_EQ(sp->subtype, T_UNDEFINED);

    push_constant_string("hello");
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_string() && lpc::svalue_view::from(sp).is_constant());
    ASSERT_STREQ(lpc::svalue_view::from(sp).c_str(), "hello");

    // Stack pointer should have moved up by 4 svalue_ts
    ASSERT_EQ(sp, initial_sp + 4);

    pop_n_elems(4);
    ASSERT_EQ(sp, initial_sp); // back to initial position
}

TEST_F(StackMachineTest, pushValueAlloc) {
    ASSERT_TRUE(sp != nullptr); // stack pointer initialized
    svalue_t *initial_sp = sp;

    copy_and_push_string("dynamic string");
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_string() && lpc::svalue_view::from(sp).is_malloc());
    ASSERT_STREQ(lpc::svalue_view::from(sp).c_str(), "dynamic string");
    EXPECT_FALSE(findstring("dynamic string", NULL)); // a private malloced string, not shared

    share_and_push_string("shared string");
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_string() && lpc::svalue_view::from(sp).is_shared());
    ASSERT_STREQ(lpc::svalue_view::from(sp).c_str(), "shared string");
    EXPECT_EQ(findstring("shared string", NULL), lpc::svalue_view::from(sp).shared_string()); // a shared string

    share_and_push_string("shared string");
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_string() && lpc::svalue_view::from(sp).is_shared());
    ASSERT_EQ(lpc::svalue_view::from(sp).shared_string(), findstring("shared string", NULL)); // pushing the same shared string again should give the same pointer
    ASSERT_EQ(lpc::svalue_view::from(sp).shared_string(), lpc::svalue_view(sp - 1).shared_string());

    pop_n_elems(3);
    ASSERT_EQ(sp, initial_sp); // back to initial position
}
