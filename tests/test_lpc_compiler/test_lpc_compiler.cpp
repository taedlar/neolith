#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

using namespace testing;

TEST_F(LPCCompilerTest, compileFile) {
    // compile a simple test file
    int fd = FILE_OPEN("master.c", O_RDONLY);
    ASSERT_NE(fd, -1) << "Failed to open master.c for reading.";
    program_t* prog = compile_file(fd, "master.c", 0);
    ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";
    total_lines = 0;
    FILE_CLOSE(fd);

    // free the compiled program
    free_prog(prog, 1);
}

TEST_F(LPCCompilerTest, loadMaster)
{
    init_simulate();
    init_master (CONFIG_STR (__MASTER_FILE__));
    ASSERT_TRUE(master_ob != nullptr) << "master_ob is null after init_master().";
    // master_ob should have ref count 2: one from set_master, one from get_empty_object
    EXPECT_EQ(master_ob->ref, 2) << "master_ob reference count is not 2 after init_master().";

    // calling destruct_object() on master_ob or simul_efun_ob effectively destroys currently
    // loaded master/simul_efun object and reloads the latest one from the file.
    object_t* old_master_ob = master_ob;
    current_object = master_ob;
    destruct_object (master_ob);
    EXPECT_NE(master_ob, old_master_ob) << "master_ob was not reloaded after destruct_object(master_ob).";
    EXPECT_EQ(master_ob->ref, 2) << "master_ob reference count is not 2 after destruct_object(master_ob).";
    tear_down_simulate();
}

TEST_F(LPCCompilerTest, loadObject) {
    init_simulate();
    init_simul_efun (CONFIG_STR (__SIMUL_EFUN_FILE__));

    // master_ob must be initialized before load_object can be used
    init_master (CONFIG_STR (__MASTER_FILE__));
    ASSERT_TRUE(master_ob != nullptr) << "master_ob is null after init_master().";

    // load a nonexistent object
    current_object = master_ob;
    object_t* obj = load_object("nonexistent_file.c", 0);
    EXPECT_TRUE(obj == nullptr) << "load_object() did not return null for nonexistent file.";

    // load an existing object
    obj = load_object("user.c", 0);
    ASSERT_TRUE(obj != nullptr) << "load_object() returned null for user.c.";
    // the object name removes leading slash and trailing ".c"
    EXPECT_STREQ(obj->name, "user") << "Loaded object name mismatch.";
    destruct_object(obj);
    EXPECT_TRUE(obj->flags & O_DESTRUCTED) << "Object not marked destructed after destruct_object().";

    obj = load_object("path/to/test_object.c", "// Pre-text for testing\nvoid create() {}\n");
    ASSERT_TRUE(obj != nullptr) << "load_object() returned null for test_object.c with pre-text.";
    EXPECT_STREQ(obj->name, "path/to/test_object") << "Loaded object name mismatch.";
    destruct_object(obj);
    EXPECT_TRUE(obj->flags & O_DESTRUCTED) << "Object not marked destructed after destruct_object().";
    
    tear_down_simulate();
}
