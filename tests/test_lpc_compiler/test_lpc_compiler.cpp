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
    setup_simulate();
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
    setup_simulate();
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

TEST_F(LPCCompilerTest, programAlignment) {
    // Verify that compiled programs have correct pointer alignment for both 32-bit and 64-bit platforms.
    // The align() macro in compiler.h must ensure 8-byte alignment on 64-bit, 4-byte on 32-bit.
    
    setup_simulate();
    init_simul_efun(CONFIG_STR(__SIMUL_EFUN_FILE__));
    init_master(CONFIG_STR(__MASTER_FILE__));
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
    
    destruct_object(obj);
    tear_down_simulate();
}
