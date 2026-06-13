#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"
#include <fstream>
#include <system_error>

TEST_F(EfunsTest, getDirDirectoryPathReturnsAllEntries) {
    std::error_code ec;
    namespace fs = std::filesystem;

    const char *mudlib_root = MAIN_OPTION(mudlib_dir_absolute);
    ASSERT_NE(mudlib_root, nullptr);
    ASSERT_NE(mudlib_root[0], '\0');

    fs::path temp_dir = fs::path(mudlib_root) / "tmp_get_dir_regression";
    fs::remove_all(temp_dir, ec);
    ec.clear();
    ASSERT_TRUE(fs::create_directories(temp_dir, ec)) << "Failed to create temp dir: " << ec.message();

    fs::path alpha = temp_dir / "alpha.txt";
    fs::path beta = temp_dir / "beta.txt";
    ASSERT_TRUE(fs::exists(alpha.parent_path()));
    ASSERT_TRUE(std::ofstream(alpha.string()) << "a");
    ASSERT_TRUE(std::ofstream(beta.string()) << "b");

    object_t* obj = load_object("/tests/efuns/test_get_dir", R"(
        mixed run(string path) { return get_dir(path); }
    )");
    ASSERT_NE(obj, nullptr) << "Failed to load get_dir test object";

    current_object = obj;
    push_constant_string("/tmp_get_dir_regression");
    svalue_t *ret = APPLY_SLOT_CALL("run", obj, 1, ORIGIN_DRIVER);
    ASSERT_NE(ret, nullptr) << "run() apply failed";

    auto ret_view = lpc::svalue_view::from(ret);
    ASSERT_TRUE(ret_view.is_array()) << "Expected get_dir() to return an array";
    ASSERT_NE(ret->u.arr, nullptr);
    ASSERT_GE(ret->u.arr->size, 2) << "Expected both files from directory listing";

    bool saw_alpha = false;
    bool saw_beta = false;
    for (int i = 0; i < ret->u.arr->size; i++) {
        auto item = lpc::svalue_view::from(&ret->u.arr->item[i]);
        ASSERT_TRUE(item.is_string()) << "Expected array items to be strings";
        if (strcmp(item.c_str(), "alpha.txt") == 0) {
            saw_alpha = true;
        }
        else if (strcmp(item.c_str(), "beta.txt") == 0) {
            saw_beta = true;
        }
    }

    EXPECT_TRUE(saw_alpha);
    EXPECT_TRUE(saw_beta);

    APPLY_SLOT_FINISH_CALL();
    destruct_object(obj);
    fs::remove_all(temp_dir, ec);
}

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
