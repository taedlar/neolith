#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

using namespace testing;

TEST_F(StackMachineTest, initialState) {
    // =================================================
    // stack machine initial state: see reset_interpreter()
    // =================================================
    ASSERT_EQ(mud_state(), MS_PRE_MUDLIB);
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

TEST_F(StackMachineTest, typedPushHelpersPreserveSubtypeContracts) {
    ASSERT_TRUE(sp != nullptr);
    svalue_t *initial_sp = sp;

    shared_str_t shared = make_shared_string("stack shared", NULL);
    unsigned short shared_refs_before = COUNTED_REF(shared);

    push_shared_string(shared);
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_string() && lpc::svalue_view::from(sp).is_shared());
    ASSERT_EQ(lpc::svalue_view::from(sp).shared_string(), shared);
    EXPECT_EQ(COUNTED_REF(shared), static_cast<unsigned short>(shared_refs_before + 1));

    pop_stack();
    ASSERT_EQ(sp, initial_sp);
    EXPECT_EQ(COUNTED_REF(shared), shared_refs_before);
    free_string(to_shared_str(shared));

    malloc_str_t malloced = new_string(5, "stack-test");
    memcpy(malloced, "hello", 5);
    malloced[5] = '\0';

    push_malloced_string(malloced);
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_string() && lpc::svalue_view::from(sp).is_malloc());
    ASSERT_EQ(lpc::svalue_view::from(sp).malloc_string(), malloced);

    pop_stack();
    ASSERT_EQ(sp, initial_sp);
}

TEST_F(StackMachineTest, typedPutMacrosPreserveSubtypeContracts) {
    ASSERT_TRUE(sp != nullptr);
    svalue_t *initial_sp = sp;

    push_undefined();
    shared_str_t shared = make_shared_string("put shared", NULL);
    put_shared_string(shared);
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_string() && lpc::svalue_view::from(sp).is_shared());
    EXPECT_EQ(lpc::svalue_view::from(sp).shared_string(), shared);
    pop_stack();
    ASSERT_EQ(sp, initial_sp);

    push_undefined();
    malloc_str_t malloced = new_string(3, "put-test");
    memcpy(malloced, "abc", 3);
    malloced[3] = '\0';
    put_malloced_string(malloced);
    ASSERT_TRUE(lpc::svalue_view::from(sp).is_string() && lpc::svalue_view::from(sp).is_malloc());
    EXPECT_EQ(lpc::svalue_view::from(sp).malloc_string(), malloced);
    EXPECT_STREQ(lpc::svalue_view::from(sp).c_str(), "abc");
    pop_stack();
    ASSERT_EQ(sp, initial_sp);
}

TEST_F(StackMachineTest, typedPushSharedStringAcceptsEmptySharedPayload) {
    ASSERT_TRUE(sp != nullptr);
    svalue_t *initial_sp = sp;

    shared_str_t empty = make_shared_string("", NULL);
    ASSERT_NE(empty, nullptr);

    push_shared_string(empty);
    auto view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(view.is_string() && view.is_shared());
    EXPECT_EQ(view.shared_string(), empty);
    EXPECT_STREQ(view.c_str(), "");

    pop_stack();
    ASSERT_EQ(sp, initial_sp);
    free_string(to_shared_str(empty));
}
