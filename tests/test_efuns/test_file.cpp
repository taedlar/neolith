#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"
#include <system_error>

TEST_F(EfunsTest, saveObject) {
    std::error_code ec;
    namespace fs = std::filesystem;
    char save_file_path[] = "test_save_object.o";
    fs::remove(save_file_path, ec); // ensure no leftover file from previous runs
    object_t* obj = load_object("/tests/efuns/test_save_object", R"(
        // object with variables
        int x;
        void create() { x = 42; }
    )");
    ASSERT_NE(obj, nullptr) << "Failed to load test object";

    // Save the object state to a file
    current_object = obj;
    st_num_arg = 2;
    push_constant_string(save_file_path);
    push_number(0); // flag to indicate saving variables
    f_save_object(); // FIXME: leaks one counted string

    auto view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(view.is_number());
    ASSERT_EQ(view.number(), 1) << "Failed to save object state";
    destruct_object(obj);

    EXPECT_TRUE (fs::exists(save_file_path, ec)) << "Save file was not created";

    // load a new object for restoring state
    obj = load_object("/tests/efuns/test_save_object", R"(
        // object with variables
        int x = 0;
        int get_number() { return x; }
    )");
    ASSERT_NE(obj, nullptr) << "Failed to load test object for restore";

    // Restore the object state from the file
    current_object = obj;
    st_num_arg = 1;
    push_constant_string(save_file_path);
    f_restore_object();
    view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(view.is_number());
    ASSERT_EQ(view.number(), 1) << "Failed to restore object state";

    // Verify that the variable 'x' was restored correctly
    EXPECT_NE(APPLY_SLOT_CALL("get_number", obj, 0, ORIGIN_DRIVER), nullptr) << "Failed to call get_number after restore";
    view = lpc::svalue_view::from(sp);
    ASSERT_TRUE(view.is_number());
    ASSERT_EQ(view.number(), 42) << "Restored variable value is incorrect";
    APPLY_SLOT_FINISH_CALL();
}
