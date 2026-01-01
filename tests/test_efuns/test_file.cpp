#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

TEST_F(EfunsTest, saveObject) {
    namespace fs = std::filesystem;
    char save_file_path[] = "test_save_object.o";
    object_t* obj = load_object("/tests/efuns/test_save_object",
        "// object with variables\n"
        "int x;\n"
        "void create() { x = 42; }\n"
    );
    ASSERT_NE(obj, nullptr) << "Failed to load test object";

    // Save the object state to a file
    current_object = obj;
    st_num_arg = 2;
    push_constant_string(save_file_path);
    push_number(0); // flag to indicate saving variables
    f_save_object();

    ASSERT_TRUE(sp->type == T_NUMBER);
    ASSERT_EQ(sp->u.number, 1) << "Failed to save object state";
    destruct_object(obj);

    EXPECT_TRUE (fs::exists(save_file_path)) << "Save file was not created";

    // load a new object for restoring state
    obj = load_object("/tests/efuns/test_save_object",
        "// object with variables\n"
        "int x = 0;\n"
        "int get_number() { return x; }\n"
    );
    ASSERT_NE(obj, nullptr) << "Failed to load test object for restore";

    // Restore the object state from the file
    current_object = obj;
    st_num_arg = 1;
    push_constant_string(save_file_path);
    f_restore_object();
    ASSERT_TRUE(sp->type == T_NUMBER);
    ASSERT_EQ(sp->u.number, 1) << "Failed to restore object state";

    // Verify that the variable 'x' was restored correctly
    apply_low("get_number", obj, 0);
    ASSERT_TRUE(sp->type == T_NUMBER);
    ASSERT_EQ(sp->u.number, 42) << "Restored variable value is incorrect";
    pop_stack();

    destruct_object(obj);
}
