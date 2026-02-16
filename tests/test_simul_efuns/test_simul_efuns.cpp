#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include <gtest/gtest.h>
#include <filesystem>
extern "C" {
    #include "std.h"
    #include "rc.h"
    #include "src/simul_efun.h"
    #include "uids.h"
    #include "lpc/lex.h"
    #include "lpc/object.h"
}

using namespace testing;

class SimulEfunsTest: public Test {
private:
    std::filesystem::path previous_cwd;

protected:
    /*  SimulEfunsTest::SetUp()
     *  --------------------------------
     *  Initialize the Simul Efun environment for testing.
     * 
     *  The master object and simul_efun object are NOT loaded here.
     *  --------------------------------
     */
    void SetUp() override {
        debug_set_log_with_date (0);
        setlocale(LC_ALL, PLATFORM_UTF8_LOCALE); // force UTF-8 locale for consistent string handling
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
        init_lpc_compiler(CONFIG_INT (__MAX_LOCAL_VARIABLES__), CONFIG_STR (__INCLUDE_DIRS__));

        setup_simulate();
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

TEST_F(SimulEfunsTest, loadSimulEfun)
{
    ASSERT_EQ(get_machine_state(), MS_PRE_MUDLIB);
    init_simul_efun ("/simul_efun.c");
    ASSERT_TRUE(simul_efun_ob != nullptr) << "simul_efun_ob is null after init_simul_efun().";
    // simul_efun_ob should have ref count 2: one from set_simul_efun, one from get_empty_object
    EXPECT_EQ(simul_efun_ob->ref, 2) << "simul_efun_ob reference count is not 2 after init_simul_efun().";

    // simul_efun_ob should be granted NONAME uid without master object.
    EXPECT_STREQ(simul_efun_ob->uid->name, "NONAME");

    // simul_efun_base should still be loaded
    object_t* base_ob = find_object_by_name("api/unicode");
    EXPECT_TRUE(base_ob != nullptr);
    destruct_object(base_ob);
}

TEST_F(SimulEfunsTest, protectSimulEfun)
{
    ASSERT_EQ(get_machine_state(), MS_PRE_MUDLIB);
    init_simul_efun ("/simul_efun.c");
    ASSERT_TRUE(simul_efun_ob != nullptr) << "simul_efun_ob is null after init_simul_efun().";
    // simul_efun_ob should have ref count 2: one from set_simul_efun, one from get_empty_object
    EXPECT_EQ(simul_efun_ob->ref, 2) << "simul_efun_ob reference count is not 2 after init_simul_efun().";

    init_master ("/master.c");
    ASSERT_TRUE(master_ob != nullptr) << "master_ob is null after init_master().";

    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
        debug_message("***** expected error: destruct simul_efun_ob while master object exists.");
        pop_context (&econ);
        return;
    }
    else {
        current_object = master_ob;
        destruct_object (simul_efun_ob); // should raise error
        /*
         * The reason to prevent simul_efun_ob from being destructed while master_ob exists:
         * 1. The order of function definitions in simul_efun_ob is used as simul_num in the permanent identifier table.
         * 2. When a LPC object that calls simul_efun is loaded, the corresponding simul_num is stored in the compiled
         *    opcode F_SIMUL_EFUN at compile time.
         * 3. If simul_efun_ob is destructed and reloaded, the order of function definitions may change,
         *    causing the simul_num to refer to a different function than intended.
         * 
         * In original LPMud and MudOS, the simul_efun_ob is never destructed once loaded.
         * Neolith allows destructing simul_efun_ob only when master_ob does not exist, which can only be triggered by
         * using the --pedantic option that enables subsystem teardown when the driver.
         */
    }
    pop_context (&econ);
    FAIL() << "destruct_object(simul_efun_ob) did not raise error when master object exists.";

    object_t* base_ob = find_object_by_name("api/unicode");
    EXPECT_TRUE(base_ob != nullptr);
    destruct_object(base_ob);
}

TEST_F(SimulEfunsTest, findSimulEfun)
{
    ASSERT_EQ(get_machine_state(), MS_PRE_MUDLIB);
    init_simul_efun ("/simul_efun.c");
    ASSERT_TRUE(simul_efun_ob != nullptr) << "simul_efun_ob is null after init_simul_efun().";
    // simul_efun_ob should have ref count 2: one from set_simul_efun, one from get_empty_object
    EXPECT_EQ(simul_efun_ob->ref, 2) << "simul_efun_ob reference count is not 2 after init_simul_efun().";

    char* func_name = findstring("textwrap");
    ASSERT_TRUE(func_name != nullptr) << "Failed to find string 'textwrap'.";
    EXPECT_NE(find_simul_efun(func_name), -1) << "find_simul_efun failed to find 'textwrap'.";

    ident_hash_elem_t* ihe = lookup_ident(func_name);
    ASSERT_TRUE(ihe != nullptr) << "lookup_ident failed to find 'textwrap'.";
    EXPECT_TRUE(ihe->token & IHE_SIMUL) << "'textwrap' ident_hash_elem_t does not have IHE_SIMUL flag set.";

    func_name = findstring("create"); // create() is always attempted when loading an object
    ASSERT_TRUE(func_name != nullptr) << "Failed to find string 'create'.";
    EXPECT_EQ(find_simul_efun(func_name), -1);

    object_t* base_ob = find_object_by_name("api/unicode");
    EXPECT_TRUE(base_ob != nullptr);
    destruct_object(base_ob);
}

TEST_F(SimulEfunsTest, callSimulEfun)
{
    ASSERT_EQ(get_machine_state(), MS_PRE_MUDLIB);
    init_simul_efun ("/simul_efun.c");
    ASSERT_TRUE(simul_efun_ob != nullptr) << "simul_efun_ob is null after init_simul_efun().";
    // simul_efun_ob should have ref count 2: one from set_simul_efun, one from get_empty_object
    EXPECT_EQ(simul_efun_ob->ref, 2) << "simul_efun_ob reference count is not 2 after init_simul_efun().";

    char* func_name = findstring("textwrap");
    ASSERT_TRUE(func_name != nullptr) << "Failed to find string 'textwrap'.";
    int index = find_simul_efun(func_name);
    EXPECT_NE(index, -1) << "find_simul_efun failed to find 'textwrap'.";

    current_object = simul_efun_ob;
    push_constant_string("Hello, world. This will be wrapped.");
    push_number(10);
    call_simul_efun (index, 2);
    EXPECT_TRUE(sp->type == T_STRING) << "Return value type from simul efun 'textwrap' is not T_STRING.";
    //GTEST_LOG_(INFO) << "Simul efun 'textwrap' returned: " << sp->u.string;
    EXPECT_STREQ(sp->u.string, "Hello, world.\nThis will be\nwrapped.") << "Return value from simul efun 'textwrap' is not correct.";

    pop_stack(); // pop string result

    object_t* base_ob = find_object_by_name("api/unicode");
    EXPECT_TRUE(base_ob != nullptr);
    destruct_object(base_ob);
}
