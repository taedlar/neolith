#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

using namespace testing;

TEST_F(LPCCompilerTest, compileFile) {
    namespace fs = std::filesystem;
    // without setup_simulate(), current directory is configuration directory initialized
    // by test fixture.
    EXPECT_TRUE(MAIN_OPTION(mudlib_dir_absolute)[0] == '\0')
        << "MAIN_OPTION(mudlib_dir_absolute) should be empty before setup_simulate()";
    fs::path path = fs::current_path() / CONFIG_STR(__MUD_LIB_DIR__) / "master.c";
    ASSERT_TRUE(fs::exists(path)) << "Test file does not exist: " << path;

    // compile_file() is a low-level API that compiles a file and returns the compiled
    // program structure. It does not rely on CWD being set to the mudlib directory,
    // but it does require the file path to be correct.
    int fd = FILE_OPEN(path.u8string().c_str(), O_RDONLY);
    ASSERT_NE(fd, -1) << "Failed to open master.c for reading.";
    program_t* prog = compile_file (fd, "master.c", 0);
    ASSERT_TRUE(prog != nullptr) << "compile_file returned null program.";
    total_lines = 0;
    FILE_CLOSE(fd);

    // free the compiled program
    free_prog(prog, 1);
}

TEST_F(LPCCompilerTest, loadMaster) {
    setup_simulate();
    init_master (CONFIG_STR (__MASTER_FILE__), NULL);
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
    setup_simulate();

    // master_ob must be initialized before load_object can be used
    init_master (CONFIG_STR (__MASTER_FILE__), NULL);
    ASSERT_NE(master_ob, nullptr) << "master_ob is null after init_master().";

    // load a nonexistent object
    current_object = master_ob;
    object_t* obj = load_object("/path/to/non-existing/object.c", 0);
    EXPECT_EQ(obj, nullptr) << "load_object() did not return null for non-existent file.";

    // load_object must reject invalid traversal paths outside m3_mudlib
    error_context_t econ;
    bool invalid_path_rejected = false;
    save_context(&econ);
    try {
        obj = load_object("../user.c", 0);
        invalid_path_rejected = (obj == nullptr);
    }
    catch (const neolith::driver_runtime_error &) {
        restore_context(&econ);
        invalid_path_rejected = true;
    }
    pop_context(&econ);
    EXPECT_TRUE(invalid_path_rejected) << "load_object() did not reject invalid path traversal.";

    // load an existing object
    obj = load_object("user.c", 0);
    ASSERT_NE(obj, nullptr) << "load_object() returned null for user.c.";
    // the object name removes leading slash and trailing ".c"
    EXPECT_STREQ(obj->name, "user") << "Loaded object name mismatch.";

    // load an object with pre-text (source file is optional if pre-text is provided)
    obj = load_object("path/to/test_object.c", "// Pre-text for testing\nvoid create() {}\n");
    ASSERT_NE(obj, nullptr) << "load_object() unable to load with pre-text.";
    EXPECT_STREQ(obj->name, "path/to/test_object") << "Loaded object name mismatch.";

    // clean up all loaded objects automatically
    tear_down_simulate();
}

TEST_F(LPCCompilerTest, loadObjectUsesVerifiedMudlibPathOutsideMudlibCwd) {
    namespace fs = std::filesystem;

    setup_simulate();
    init_master (CONFIG_STR (__MASTER_FILE__), NULL);
    ASSERT_NE(master_ob, nullptr) << "master_ob is null after init_master().";

    const fs::path mudlib_cwd = fs::current_path();
    const fs::path shifted_cwd = mudlib_cwd.parent_path();

    ASSERT_LT(mudlib_cwd.string().size(), static_cast<size_t>(PATH_MAX));
    strncpy(MAIN_OPTION(mudlib_dir_absolute), mudlib_cwd.string().c_str(), PATH_MAX - 1);
    MAIN_OPTION(mudlib_dir_absolute)[PATH_MAX - 1] = '\0';

    fs::current_path(shifted_cwd);

    current_object = master_ob;
    object_t* obj = load_object("user.c", 0);
    ASSERT_NE(obj, nullptr) << "load_object() returned null for user.c outside mudlib cwd.";
    EXPECT_STREQ(obj->name, "user") << "Loaded object name mismatch.";

    fs::current_path(mudlib_cwd);
    tear_down_simulate();
}

TEST_F(LPCCompilerTest, includeUsesVerifiedMudlibPathOutsideMudlibCwd) {
    namespace fs = std::filesystem;

    setup_simulate();
    init_master (CONFIG_STR (__MASTER_FILE__), NULL);
    ASSERT_NE(master_ob, nullptr) << "master_ob is null after init_master().";

    const fs::path mudlib_cwd = fs::current_path();
    const fs::path shifted_cwd = mudlib_cwd.parent_path();

    ASSERT_LT(mudlib_cwd.string().size(), static_cast<size_t>(PATH_MAX));
    strncpy(MAIN_OPTION(mudlib_dir_absolute), mudlib_cwd.string().c_str(), PATH_MAX - 1);
    MAIN_OPTION(mudlib_dir_absolute)[PATH_MAX - 1] = '\0';

    // This is to simulate that CWD was changed accidentally by some code
    fs::current_path(shifted_cwd);

    current_object = master_ob;
    object_t *obj = load_object("test_include_verified_path.c", R"(
        #include <config.h>
        void create() {}
    )");
    ASSERT_NE(obj, nullptr) << "load_object() failed to resolve include path outside mudlib cwd.";

    fs::current_path(mudlib_cwd);
    tear_down_simulate();
}

TEST_F(LPCCompilerTest, programAlignment) {
    // Verify that compiled programs have correct pointer alignment for both 32-bit and 64-bit platforms.
    // The align() macro in compiler.h must ensure 8-byte alignment on 64-bit, 4-byte on 32-bit.
    
    setup_simulate();

    init_master(CONFIG_STR(__MASTER_FILE__), NULL);
    ASSERT_NE(master_ob, nullptr);
    
    // Compile a test object with various data to exercise different memory blocks
    current_object = master_ob;
    const char* test_code = R"(
        // Test code to exercise different memory blocks
        string global_var1;
        int global_var2;
        mapping global_var3;
        
        class TestClass {
            string member1;
            int member2;
        }
        
        void create() {}
        
        int test_func1(string arg) { return 42; }
        string test_func2(int x, int y) { return "test"; }
        void test_func3() { 
            int local1 = 1;
            string local2 = "hello";
        }
    )";
    
    object_t* obj = load_object("test_alignment.c", test_code);
    ASSERT_NE(obj, nullptr) << "Failed to compile test object";
    
    program_t* prog = obj->prog;
    ASSERT_NE(prog, nullptr);
    
    // Verify pointer alignment requirements
    const size_t ptr_size = sizeof(void*);
    const size_t expected_alignment = ptr_size;  // Should be 8 on 64-bit, 4 on 32-bit
    
    // Check that program_t base address is properly aligned
    uintptr_t prog_addr = reinterpret_cast<uintptr_t>(prog);
    EXPECT_EQ(prog_addr % expected_alignment, 0) 
        << "program_t base address not aligned to " << expected_alignment << " bytes";
    
    // Check that all pointer fields in program_t are properly aligned
    // These must be aligned because they're accessed as pointers
    
    if (prog->program) {
        uintptr_t program_addr = reinterpret_cast<uintptr_t>(prog->program);
        EXPECT_EQ(program_addr % expected_alignment, 0)
            << "program->program not aligned to " << expected_alignment << " bytes";
    }
    
    if (prog->function_table) {
        uintptr_t func_table_addr = reinterpret_cast<uintptr_t>(prog->function_table);
        EXPECT_EQ(func_table_addr % expected_alignment, 0)
            << "program->function_table not aligned to " << expected_alignment << " bytes";
    }
    
    if (prog->function_flags) {
        uintptr_t func_flags_addr = reinterpret_cast<uintptr_t>(prog->function_flags);
        EXPECT_EQ(func_flags_addr % expected_alignment, 0)
            << "program->function_flags not aligned to " << expected_alignment << " bytes";
    }
    
    if (prog->function_offsets) {
        uintptr_t func_offsets_addr = reinterpret_cast<uintptr_t>(prog->function_offsets);
        EXPECT_EQ(func_offsets_addr % expected_alignment, 0)
            << "program->function_offsets not aligned to " << expected_alignment << " bytes";
    }
    
    if (prog->strings) {
        uintptr_t strings_addr = reinterpret_cast<uintptr_t>(prog->strings);
        EXPECT_EQ(strings_addr % expected_alignment, 0)
            << "program->strings not aligned to " << expected_alignment << " bytes";
    }
    
    if (prog->variable_table) {
        uintptr_t var_table_addr = reinterpret_cast<uintptr_t>(prog->variable_table);
        EXPECT_EQ(var_table_addr % expected_alignment, 0)
            << "program->variable_table not aligned to " << expected_alignment << " bytes";
    }
    
    if (prog->variable_types) {
        uintptr_t var_types_addr = reinterpret_cast<uintptr_t>(prog->variable_types);
        EXPECT_EQ(var_types_addr % expected_alignment, 0)
            << "program->variable_types not aligned to " << expected_alignment << " bytes";
    }
    
    if (prog->inherit) {
        uintptr_t inherit_addr = reinterpret_cast<uintptr_t>(prog->inherit);
        EXPECT_EQ(inherit_addr % expected_alignment, 0)
            << "program->inherit not aligned to " << expected_alignment << " bytes";
    }
    
    if (prog->classes) {
        uintptr_t classes_addr = reinterpret_cast<uintptr_t>(prog->classes);
        EXPECT_EQ(classes_addr % expected_alignment, 0)
            << "program->classes not aligned to " << expected_alignment << " bytes";
    }
    
    if (prog->class_members) {
        uintptr_t class_members_addr = reinterpret_cast<uintptr_t>(prog->class_members);
        EXPECT_EQ(class_members_addr % expected_alignment, 0)
            << "program->class_members not aligned to " << expected_alignment << " bytes";
    }
    
    // Verify total_size is also properly aligned (not strictly necessary but good practice)
    EXPECT_EQ(prog->total_size % expected_alignment, 0)
        << "program->total_size not aligned to " << expected_alignment << " bytes";
    
    // Log platform info for diagnostic purposes
    std::cout << "Platform: " << (ptr_size == 8 ? "64-bit" : "32-bit") 
              << ", pointer size: " << ptr_size << " bytes" << std::endl;
    std::cout << "Program total_size: " << prog->total_size << " bytes" << std::endl;

    tear_down_simulate();
}

TEST_F(LPCCompilerTest, inheritPathWithEmbeddedNullFailsCompile) {
    setup_simulate();
    init_master(CONFIG_STR(__MASTER_FILE__), NULL);
    ASSERT_NE(master_ob, nullptr);

    current_object = master_ob;
    const char *test_code =
        "inherit \"std/object\\0evil\";\n"
        "void create() {}\n";

    error_context_t econ;
    save_context (&econ);
    try {
        object_t *obj = load_object("test_inherit_embedded_null_fail.c", test_code);
        EXPECT_EQ(obj, nullptr) << "inherit with embedded null path unexpectedly compiled.";
    }
    catch (const neolith::driver_runtime_error &) {
        restore_context (&econ);
        debug_message("***** expected error: inherit path contains embedded null.");
    }

    pop_context (&econ);
    tear_down_simulate();
}

TEST_F(LPCCompilerTest, C99BlockDeclInterleavedCompiles) {
    const char *test_code = R"(
        int run_test() {
            int sum = 0;
            sum += 1;
            int x = 41;
            sum += x;
            int y = 1;
            return sum + y;
        }
    )";

    program_t *prog = compile_file(-1, "test_c99_block_decl_interleaved_ok.c", test_code);
    ASSERT_NE(prog, nullptr) << "interleaved block declarations unexpectedly failed to compile.";
    free_prog(prog, 1);
}

TEST_F(LPCCompilerTest, C99BlockDeclNestedBlockCompiles) {
    const char *test_code = R"(
        int run_test() {
            int x = 1;
            {
                int z = 2;
                z += 1;
            }
            int y = x;
            return y;
        }
    )";

    program_t *prog = compile_file(-1, "test_c99_block_decl_nested_ok.c", test_code);
    ASSERT_NE(prog, nullptr) << "nested mixed block declarations failed to compile.";
    free_prog(prog, 1);
}

TEST_F(LPCCompilerTest, C99BlockDeclInLoopBodyCompiles) {
    const char *test_code = R"(
        int run_test() {
            int i;
            int total = 0;
            for (i = 0; i < 3; i++) {
                total += i;
                int twice = i + i;
                total += twice;
            }
            return total;
        }
    )";

    program_t *prog = compile_file(-1, "test_c99_block_decl_loop_ok.c", test_code);
    ASSERT_NE(prog, nullptr) << "loop-body interleaved declarations failed to compile.";
    free_prog(prog, 1);
}

TEST_F(LPCCompilerTest, C99BlockDeclVoidFailsCompile) {
    const char *test_code = R"(
        int run_test() {
            int x = 1;
            void bad;
            return x;
        }
    )";

    program_t *prog = compile_file(-1, "test_c99_block_decl_void_fail.c", test_code);
    EXPECT_EQ(prog, nullptr) << "void local declaration unexpectedly compiled.";
    if (prog) {
        free_prog(prog, 1);
    }
}

TEST_F(LPCCompilerTest, C99BlockDeclInitializerTypeMismatchFailsCompile) {
    const char *test_code = R"(
        int run_test() {
            int x = 1;
            string s = "ok";
            int bad = s;
            return x;
        }
    )";

    program_t *prog = compile_file(-1, "test_c99_block_decl_type_mismatch_fail.c", test_code);
    EXPECT_EQ(prog, nullptr) << "type-mismatched interleaved declaration unexpectedly compiled.";
    if (prog) {
        free_prog(prog, 1);
    }
}

TEST_F(LPCCompilerTest, C99BlockDeclUseBeforeDeclarationFailsCompile) {
    const char *test_code = R"(
        int run_test() {
            int sum = 0;
            sum += x;
            int x = 1;
            return sum + x;
        }
    )";

    program_t *prog = compile_file(-1, "test_c99_block_decl_use_before_decl_fail.c", test_code);
    EXPECT_EQ(prog, nullptr) << "use-before-declaration unexpectedly compiled.";
    if (prog) {
        free_prog(prog, 1);
    }
}
