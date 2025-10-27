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
    /*  LPCCompilerTest::SetUp()
     *  --------------------------------
     *  Initialize the LPC compiler environment for testing.
     *  This includes setting up logging, locale, configuration,
     *  string management, UID management, object management,
     *  and object table.
     *  Also initializes identifiers, local variable management,
     *  include paths, instruction table, stack machine,
     *  and predefines.
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

        eval_cost = CONFIG_INT (__MAX_EVAL_COST__); /* simulates calling LPC code from backend */
    }

    void TearDown() override {
        deinit_lpc_compiler();
        deinit_strings();

        namespace fs = std::filesystem;
        fs::current_path(previous_cwd);
        deinit_config();
    }
};

TEST_F(LPCCompilerTest, compileFile) {
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        FAIL() << "Failed to compile test_file.c.";
    }
    else {
        // compile a simple test file
        int fd = open("master.c", O_RDONLY);
        ASSERT_NE(fd, -1) << "Failed to open master.c for reading.";
        program_t* prog = compile_file(fd, "master.c");
        ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";
        total_lines = 0;
        close(fd);

        // free the compiled program
        free_prog(prog, 1);
    }
    pop_context (&econ);
}

TEST_F(LPCCompilerTest, loadMaster)
{
    init_simulate();
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        debug_message("***** error occurs in loading master object.");
        // FAIL() << "Failed to load master object.";
    }
    else {
        init_master (CONFIG_STR (__MASTER_FILE__));
        ASSERT_TRUE(master_ob != nullptr) << "master_ob is null after init_master().";
        // master_ob should have ref count 2: one from set_master, one from get_empty_object
        EXPECT_EQ(master_ob->ref, 2) << "master_ob reference count is not 2 after init_master().";

        // calling destruct_object() on master_ob or simul_efun_ob effectively destroys currently
        // loaded master/simul_efun object and reloads the latest one from the file.
        object_t* old_master_ob = master_ob;
        current_object = master_ob;
        destruct_object (master_ob);
        remove_destructed_objects(); // actually free destructed objects
        EXPECT_NE(master_ob, old_master_ob) << "master_ob was not reloaded after destruct_object(master_ob).";
        EXPECT_EQ(master_ob->ref, 2) << "master_ob reference count is not 2 after destruct_object(master_ob).";
    }
    pop_context (&econ);
    tear_down_simulate();
}

TEST_F(LPCCompilerTest, loadObject) {
    init_simulate();
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        FAIL() << "Failed to load simul efuns.";
    }
    else {
        init_simul_efun (CONFIG_STR (__SIMUL_EFUN_FILE__));
        init_master (CONFIG_STR (__MASTER_FILE__));
        ASSERT_TRUE(master_ob != nullptr) << "master_ob is null after init_master().";

        current_object = master_ob;
        object_t* obj = load_object("nonexistent_file.c");
        EXPECT_TRUE(obj == nullptr) << "load_object() did not return null for nonexistent file.";

        obj = load_object("user.c");
        ASSERT_TRUE(obj != nullptr) << "load_object() returned null for user.c.";
        // the object name removes leading slash and trailing ".c"
        EXPECT_STREQ(obj->name, "user") << "Loaded object name mismatch.";

        destruct_object(obj);
        EXPECT_TRUE(obj->flags & O_DESTRUCTED) << "Object not marked destructed after destruct_object().";
    }
    pop_context (&econ);
    tear_down_simulate();
}
