#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include <gtest/gtest.h>
#include <filesystem>
extern "C" {
    #include "std.h"
    #include "rc.h"
    #include "src/simul_efun.h"
    #include "src/uids.h"
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
        setlocale(LC_ALL, "C.UTF-8"); // force UTF-8 locale for consistent string handling
        init_stem(3, (unsigned long)-1, "m3.conf"); // use highest debug level and enable all trace logs

        init_config(SERVER_OPTION(config_file));

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

TEST_F(SimulEfunsTest, loadSimulEfun)
{
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
        // FAIL() << "Failed to load simul efuns.";
    }
    else {
        ASSERT_EQ(get_machine_state(), MS_PRE_MUDLIB);
        init_simul_efun ("/simul_efun.c");
        ASSERT_TRUE(simul_efun_ob != nullptr) << "simul_efun_ob is null after init_simul_efun().";
        // simul_efun_ob should have ref count 2: one from set_simul_efun, one from get_empty_object
        EXPECT_EQ(simul_efun_ob->ref, 2) << "simul_efun_ob reference count is not 2 after init_simul_efun().";

        // simul_efun_ob should be granted NONAME uid without master object.
        EXPECT_STREQ(simul_efun_ob->uid->name, "NONAME");

        // simul_efun_base should still be loaded
        object_t* base_ob = find_object("/simul_efun_base");;
        EXPECT_TRUE(base_ob != nullptr) << "simul_efun_base object not found after destructing simul_efun_ob.";
        destruct_object(base_ob);
    }
    pop_context (&econ);
}

TEST_F(SimulEfunsTest, protectSimulEfun)
{
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
    }
    else {
        ASSERT_EQ(get_machine_state(), MS_PRE_MUDLIB);
        init_simul_efun ("/simul_efun_test.c");
        ASSERT_TRUE(simul_efun_ob != nullptr) << "simul_efun_ob is null after init_simul_efun().";
        // simul_efun_ob should have ref count 2: one from set_simul_efun, one from get_empty_object
        EXPECT_EQ(simul_efun_ob->ref, 2) << "simul_efun_ob reference count is not 2 after init_simul_efun().";

        init_master ("/master.c");
        ASSERT_TRUE(master_ob != nullptr) << "master_ob is null after init_master().";

        current_object = master_ob;
        destruct_object (simul_efun_ob); // should raise error
        FAIL() << "Simul efun object was destructed while master object exists.";
    }
    pop_context (&econ);
}

TEST_F(SimulEfunsTest, findSimulEfun)
{
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
        // FAIL() << "Failed to load simul efuns.";
    }
    else {
        ASSERT_EQ(get_machine_state(), MS_PRE_MUDLIB);
        init_simul_efun ("/simul_efun_test.c");
        ASSERT_TRUE(simul_efun_ob != nullptr) << "simul_efun_ob is null after init_simul_efun().";
        // simul_efun_ob should have ref count 2: one from set_simul_efun, one from get_empty_object
        EXPECT_EQ(simul_efun_ob->ref, 2) << "simul_efun_ob reference count is not 2 after init_simul_efun().";

        char* func_name = findstring("sum");
        ASSERT_TRUE(func_name != nullptr) << "Failed to find string 'sum'.";
        EXPECT_NE(find_simul_efun(func_name), -1) << "find_simul_efun failed to find 'sum'.";

        ident_hash_elem_t* ihe = lookup_ident(func_name);
        ASSERT_TRUE(ihe != nullptr) << "lookup_ident failed to find 'sum'.";
        EXPECT_TRUE(ihe->token & IHE_SIMUL) << "'sum' ident_hash_elem_t does not have IHE_SIMUL flag set.";

        func_name = findstring("create"); // create() is always attempted when loading an object
        ASSERT_TRUE(func_name != nullptr) << "Failed to find string 'create'.";
        EXPECT_EQ(find_simul_efun(func_name), -1);
    }
    pop_context (&econ);
}
