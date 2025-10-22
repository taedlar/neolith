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
    #include "lpc/object.h"
    #include "lpc/otable.h"
}

using namespace testing;

// according to GoogleTest FAQ, the test suite name and test name should not
// contain underscores to avoid issues on some platforms.
// https://google.github.io/googletest/faq.html#why-should-test-suite-names-and-test-names-not-contain-underscore

class LPCCompilerTest: public Test {
private:
    std::filesystem::path previous_cwd;

protected:
    void SetUp() override {
        log_message("", "[ SETUP    ]\tsetting up LPCCompilerTest\n");
        debug_set_log_with_date (1);
        setlocale(LC_ALL, "C.UTF-8"); // force UTF-8 locale for consistent string handling
        init_stem(3, 0177, "m3.conf"); // use highest debug level and enable all trace logs
        init_config(SERVER_OPTION(config_file));
        ASSERT_TRUE(CONFIG_STR(__MUD_LIB_DIR__));

        namespace fs = std::filesystem;
        auto mudlib_path = fs::path(CONFIG_STR(__MUD_LIB_DIR__)); // absolute or relattive to current dir
        if (mudlib_path.is_relative()) {
            mudlib_path = fs::current_path() / mudlib_path;
        }
        ASSERT_TRUE(fs::exists(mudlib_path)) << "Mudlib directory does not exist: " << mudlib_path;
        previous_cwd = fs::current_path();
        fs::current_path(mudlib_path); // change working directory to mudlib

        init_strings (8192, 1000000); // LPC compiler needs this since prolog()
        init_uids();          // uid management
        init_objects ();
        init_otable (CONFIG_INT (__OBJECT_HASH_TABLE_SIZE__));

        init_identifiers ();
        init_locals ();
        set_inc_list (CONFIG_STR (__INCLUDE_DIRS__));
        // init_precomputed_tables ();
        init_num_args ();
        reset_machine ();
        // init_binaries ();
        add_predefines ();

        eval_cost = CONFIG_INT (__MAX_EVAL_COST__);
        error_context_t econ;
        save_context (&econ);
        // init_simul_efun (CONFIG_STR (__SIMUL_EFUN_FILE__));
        // init_master (CONFIG_STR (__MASTER_FILE__));
        // preload_objects (0);
        pop_context (&econ);
    }
    void TearDown() override {
        log_message("", "[ TEARDOWN ]\ttearing down LPCCompilerTest\n");
        free_defines(1);    // free all defines including predefines
        deinit_num_args();  // clear instruction table
        reset_machine ();   // clear stack machine
        clear_apply_cache(); // clear shared strings referenced by apply cache
        reset_inc_list();   // free include path list
        deinit_locals();    // free local variable management structures
        deinit_identifiers(); // free all identifiers

        // TODO: destruct all objects

        deinit_otable();    // free object name hash table
        deinit_objects();   // free living name hash table
        deinit_uids();      // free all uids
        deinit_strings();

        namespace fs = std::filesystem;
        fs::current_path(previous_cwd);

        // TODO: deinit_config();
    }
};

TEST_F(LPCCompilerTest, compileFile)
{
    debug_info("mudlib dir: %s", CONFIG_STR(__MUD_LIB_DIR__));
}

TEST_F(LPCCompilerTest, loadSimulEfun)
{
    eval_cost = CONFIG_INT (__MAX_EVAL_COST__);
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        FAIL() << "Failed to load simul efuns.";
    }
    else {
        init_simul_efun (CONFIG_STR (__SIMUL_EFUN_FILE__));
    }
    pop_context (&econ);
}

TEST_F(LPCCompilerTest, loadMaster)
{
    debug_message("{}\t----- CTEST_FULL_OUTPUT -----");

    eval_cost = CONFIG_INT (__MAX_EVAL_COST__);
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        FAIL() << "Failed to load master object.";
    }
    else {
        init_master (CONFIG_STR (__MASTER_FILE__));
        ASSERT_TRUE(master_ob != nullptr) << "master_ob is null after init_master.";

        // master_ob should have ref count 2: one from set_master, one from get_empty_object
        EXPECT_EQ(master_ob->ref, 2) << "master_ob reference count is not 2 after init_master.";

        remove_object_hash (master_ob); // remove from object hash
        master_ob->flags |= O_DESTRUCTED; // mark as destructed to avoid errors in dealloc_object
        free_object (master_ob, "LPCCompilerTest::loadMaster"); // free master object
        free_object (master_ob, "LPCCompilerTest::loadMaster"); // free master object
        master_ob = nullptr;
    }
    pop_context (&econ);
}
