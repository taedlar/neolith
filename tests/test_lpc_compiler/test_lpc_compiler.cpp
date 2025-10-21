#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include <gtest/gtest.h>
#include <filesystem>

extern "C" {
    #include "std.h"
    #include "rc.h"
    #include "src/simul_efun.h"
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
        init_config("m3.conf");
        ASSERT_TRUE(CONFIG_STR(__MUD_LIB_DIR__));

        namespace fs = std::filesystem;
        auto mudlib_path = fs::path(CONFIG_STR(__MUD_LIB_DIR__));
        if (mudlib_path.is_relative()) {
            mudlib_path = fs::current_path() / mudlib_path;
        }
        ASSERT_TRUE(fs::exists(mudlib_path)) << "Mudlib directory does not exist: " << mudlib_path;
        previous_cwd = fs::current_path();
        fs::current_path(mudlib_path); // change working directory to mudlib

        // init_strings ();
        // init_objects ();
        // init_otable (CONFIG_INT (__OBJECT_HASH_TABLE_SIZE__));
        // init_identifiers ();
        // init_locals ();
        // set_inc_list (CONFIG_STR (__INCLUDE_DIRS__));
        // init_precomputed_tables ();
        // init_num_args ();
        // reset_machine ();
        // init_binaries ();
        // add_predefines ();

        eval_cost = CONFIG_INT (__MAX_EVAL_COST__);
        error_context_t econ;
        save_context (&econ);
        // init_simul_efun (CONFIG_STR (__SIMUL_EFUN_FILE__));
        // init_master (CONFIG_STR (__MASTER_FILE__));
        // preload_objects (0);
        pop_context (&econ);
    }
    void TearDown() override {
        namespace fs = std::filesystem;
        fs::current_path(previous_cwd);
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
