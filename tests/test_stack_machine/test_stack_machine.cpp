#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include <gtest/gtest.h>
#include <filesystem>

extern "C" {
    #include "std.h"
    #include "rc.h"
    #include "lpc/compiler.h"
}

using namespace testing;

class StackMachineTest: public Test {
private:
    std::filesystem::path previous_cwd;

protected:
    void SetUp() override {
        debug_set_log_with_date (0);
        setlocale(LC_ALL, "C.UTF-8"); // force UTF-8 locale for consistent string handling
        init_stem(3, (unsigned long)-1, "m3.conf"); // use highest debug level and enable all trace logs

        init_config(MAIN_OPTION(config_file));

        debug_message("[ SETUP    ] CTEST_FULL_OUTPUT");
        ASSERT_TRUE(CONFIG_STR(__MUD_LIB_DIR__));
        namespace fs = std::filesystem;
        auto mudlib_path = fs::path(CONFIG_STR(__MUD_LIB_DIR__)); // absolute or relative to cwd
        if (mudlib_path.is_relative()) {
            mudlib_path = fs::current_path() / mudlib_path;
        }
        ASSERT_TRUE(fs::exists(mudlib_path)) << "Mudlib directory does not exist: " << mudlib_path;
        previous_cwd = fs::current_path();
        fs::current_path(mudlib_path); // change working directory to mudlib

        init_strings (8192, 1000000); // LPC compiler needs this since prolog()
        init_lpc_compiler(CONFIG_INT (__MAX_LOCAL_VARIABLES__));
        set_inc_list (CONFIG_STR (__INCLUDE_DIRS__)); // automatically freed in deinit_lpc_compiler()

        init_simulate();
        eval_cost = CONFIG_INT (__MAX_EVAL_COST__); /* simulates calling LPC code from backend */
    }

    void TearDown() override {
        tear_down_simulate();
        deinit_lpc_compiler();
        deinit_strings();

        namespace fs = std::filesystem;
        fs::current_path(previous_cwd);
        deinit_config();
    }
};

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
