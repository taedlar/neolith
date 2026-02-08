#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fixtures.hpp"

extern "C" {
    #include "lpc/functional.h"
    #include "lpc/include/function.h"
}

class SentenceTest : public LPCInterpreterTest {};

TEST_F(SentenceTest, MakeLfunFunpByName) {
    // Load an object with a known function
    const char* lpc_code = 
        "int add(int a, int b) { return a + b; }\n";
    
    current_object = master_ob;
    object_t* obj = load_object("test_obj.c", lpc_code);
    ASSERT_NE(obj, nullptr);
    
    // Set current_object to the loaded object
    current_object = obj;
    
    // Create a function pointer by name
    svalue_t dummy;
    dummy.type = T_NUMBER;
    dummy.u.number = 0;
    funptr_t* funp = make_lfun_funp_by_name("add", &dummy);
    
    ASSERT_NE(funp, nullptr) << "Function pointer should be created successfully";
    EXPECT_EQ(funp->hdr.type, FP_LOCAL | FP_NOT_BINDABLE);
    EXPECT_EQ(funp->hdr.owner, obj);
    EXPECT_EQ(funp->hdr.ref, 1);
    EXPECT_EQ(funp->hdr.args, nullptr);
    
    // Clean up
    free_funp(funp);
    destruct_object(obj);
}

TEST_F(SentenceTest, MakeLfunFunpByNameNonExistent) {
    // Load a simple object
    const char* lpc_code = "void create() { }\n";
    
    current_object = master_ob;
    object_t* obj = load_object("test_obj.c", lpc_code);
    ASSERT_NE(obj, nullptr);
    
    current_object = obj;
    
    // Try to create funptr for non-existent function
    svalue_t dummy;
    dummy.type = T_NUMBER;
    dummy.u.number = 0;
    funptr_t* funp = make_lfun_funp_by_name("nonexistent", &dummy);
    
    EXPECT_EQ(funp, nullptr) << "Function pointer should be NULL for non-existent function";
    
    // Clean up
    destruct_object(obj);
}

TEST_F(SentenceTest, MakeLfunFunpByNameInherited) {
    // Create a parent object
    const char* parent_code = 
        "int parent_func(int x) { return x * 2; }\n";
    
    current_object = master_ob;
    object_t* parent_obj = load_object("parent.c", parent_code);
    ASSERT_NE(parent_obj, nullptr);
    
    // Create a child object that inherits from parent
    const char* child_code = 
        "inherit \"parent\";\n"
        "int child_func(int x) { return parent_func(x) + 1; }\n";
    
    object_t* child_obj = load_object("child.c", child_code);
    ASSERT_NE(child_obj, nullptr);
    
    current_object = child_obj;
    
    // Create funptr for inherited function
    svalue_t dummy;
    dummy.type = T_NUMBER;
    dummy.u.number = 0;
    funptr_t* funp = make_lfun_funp_by_name("parent_func", &dummy);
    
    ASSERT_NE(funp, nullptr) << "Function pointer should be created for inherited function";
    EXPECT_EQ(funp->hdr.type, FP_LOCAL | FP_NOT_BINDABLE);
    EXPECT_EQ(funp->hdr.owner, child_obj);
    
    // Clean up
    free_funp(funp);
    destruct_object(child_obj);
    destruct_object(parent_obj);
}

TEST_F(SentenceTest, MakeLfunFunpByNameWithBoundArgs) {
    // Load an object with a function
    const char* lpc_code = 
        "int multiply(int a, int b) { return a * b; }\n";
    
    current_object = master_ob;
    object_t* obj = load_object("test_obj.c", lpc_code);
    ASSERT_NE(obj, nullptr);
    
    current_object = obj;
    
    // Create array with bound args
    array_t* args = allocate_empty_array(1);
    args->item[0].type = T_NUMBER;
    args->item[0].u.number = 5;
    
    svalue_t args_sval;
    args_sval.type = T_ARRAY;
    args_sval.u.arr = args;
    funptr_t* funp = make_lfun_funp_by_name("multiply", &args_sval);
    
    ASSERT_NE(funp, nullptr);
    EXPECT_EQ(funp->hdr.args, args);
    EXPECT_EQ(funp->hdr.args->ref, 2); // 1 for args array, 1 for funptr
    
    // Clean up
    free_funp(funp);
    free_array(args);
    destruct_object(obj);
}
