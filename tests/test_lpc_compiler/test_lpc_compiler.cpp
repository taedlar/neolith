#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

using namespace testing;

TEST_F(LPCCompilerTest, compileFile) {
    error_context_t econ;
    save_context (&econ);
    if (setjmp(econ.context)) {
        restore_context (&econ);
        // FAIL() << "Failed to compile test_file.c.";
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
        restore_context (&econ);
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
        restore_context (&econ);
        // FAIL() << "Failed to load simul efuns.";
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
