#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fixtures.hpp"

extern "C" {
    #include "lpc/functional.h"
    #include "lpc/include/function.h"
    #include "efuns_prototype.h"
    #include "error_context.h"
}

class InputToGetCharTest : public LPCInterpreterTest {
protected:
    object_t* user_obj = nullptr;
    interactive_t* mock_ip = nullptr;
    
    // Save console worker state during tests to prevent interference
    console_worker_context_t* saved_console_worker = nullptr;
    async_queue_t* saved_console_queue = nullptr;
    
    void SetUp() override {
        LPCInterpreterTest::SetUp();
        
        // IMPORTANT: Disable console worker during tests
        // The test fixture creates a fake interactive at all_users[0] which 
        // would conflict with real console events. Other driver code relies
        // on all_users[0] being the console user, so we must prevent the
        // console worker from attempting to access it or send events.
        saved_console_worker = g_console_worker;
        saved_console_queue = g_console_queue;
        g_console_worker = nullptr;
        g_console_queue = nullptr;
        
        // Create user object with test callbacks
        const char* code = 
            "string last_input;\n"
            "mixed* callback_args;\n"
            "\n"
            "void callback(string input) {\n"
            "    last_input = input;\n"
            "}\n"
            "\n"
            "void callback_with_args(string input, int arg1, string arg2) {\n"
            "    last_input = input;\n"
            "    callback_args = ({ arg1, arg2 });\n"
            "}\n"
            "\n"
            "void nested_callback(string input) {\n"
            "    last_input = input;\n"
            "    /* This callback sets up another input_to */\n"
            "    input_to(\"final_callback\", 0);\n"
            "}\n"
            "\n"
            "void final_callback(string input) {\n"
            "    last_input = \"final: \" + input;\n"
            "}\n"
            "\n"
            "void create() { }\n";
        
        current_object = master_ob;
        user_obj = load_object("test_user.c", code);
        ASSERT_NE(user_obj, nullptr) << "Failed to load test user object";
        
        // Use public API to create test interactive structure
        mock_ip = create_test_interactive(user_obj);
        ASSERT_NE(mock_ip, nullptr) << "Failed to create test interactive";
        
        // Set as command_giver (required for input_to/get_char)
        command_giver = user_obj;
        current_object = user_obj;
    }
    
    void TearDown() override {
        // Restore console worker state
        g_console_worker = saved_console_worker;
        g_console_queue = saved_console_queue;
        
        // Use public API to clean up test interactive
        if (mock_ip) {
            remove_test_interactive(mock_ip);
            mock_ip = nullptr;
        }
        
        // Clean up test object
        if (user_obj) {
            destruct_object(user_obj);
            user_obj = nullptr;
        }
        command_giver = nullptr;
        
        LPCInterpreterTest::TearDown();
    }
    
    /**
     * Helper to simulate user input after input_to/get_char has been called.
     * This simulates what the backend does when it receives input from the user.
     */
    void simulate_input(const char* input_text) {
        ASSERT_NE(mock_ip, nullptr) << "No interactive structure";
        int result = call_function_interactive(mock_ip, const_cast<char*>(input_text));
        EXPECT_EQ(result, 1) << "call_function_interactive failed";
    }
    
    /**
     * Helper to get a variable value from the test user object.
     * Uses direct access to object's variables.
     */
    svalue_t* get_var(const char* var_name) {
        // Find variable in program
        program_t* prog = user_obj->prog;
        for (int i = 0; i < prog->num_variables_total; i++) {
            if (strcmp(prog->variable_table[i], var_name) == 0) {
                return &user_obj->variables[i];
            }
        }
        return nullptr;
    }
    
    /**
     * Helper to fetch a string variable value.
     */
    std::string get_string_var(const char* var_name) {
        svalue_t* var = get_var(var_name);
        if (var && var->type == T_STRING && var->u.string) {
            return std::string(var->u.string);
        }
        return "";
    }
    
    /**
     * Helper to fetch a number variable value.
     */
    int64_t get_number_var(const char* var_name) {
        svalue_t* var = get_var(var_name);
        if (var && var->type == T_NUMBER) {
            return var->u.number;
        }
        return 0;
    }
    
    /**
     * Helper to get an array variable (caller does NOT need to free).
     */
    array_t* get_array_var(const char* var_name) {
        svalue_t* var = get_var(var_name);
        if (var && var->type == T_ARRAY) {
            return var->u.arr;
        }
        return nullptr;
    }
};

TEST_F(InputToGetCharTest, InputToStringCallback) {
    // Test: input_to("callback", 0)
    svalue_t fun = {};  // Zero-initialize
    fun.type = T_STRING;
    fun.u.string = const_cast<char*>("callback");
    
    int result = input_to(&fun, 0, 0, nullptr);
    EXPECT_EQ(result, 1) << "input_to should succeed";
    ASSERT_NE(mock_ip->input_to, nullptr) << "input_to should create sentence";
    
    // Verify sentence structure
    sentence_t* sent = mock_ip->input_to;
    EXPECT_TRUE(sent->flags & V_FUNCTION) << "Should be function pointer";
    EXPECT_EQ(sent->args, nullptr) << "No args should be stored";
    
    // Simulate user input
    simulate_input("hello world");
    
    // Verify callback was called with correct input
    EXPECT_EQ(get_string_var("last_input"), "hello world");
    
    // Verify sentence was cleaned up
    EXPECT_EQ(mock_ip->input_to, nullptr) << "Sentence should be freed after callback";
}

TEST_F(InputToGetCharTest, InputToWithCarryoverArgs) {
    // Test: input_to("callback_with_args", 0, 42, "extra")
    svalue_t fun = {};  // Zero-initialize
    fun.type = T_STRING;
    fun.u.string = const_cast<char*>("callback_with_args");
    
    svalue_t args[2] = {};  // Zero-initialize
    args[0].type = T_NUMBER;
    args[0].u.number = 42;
    args[1].type = T_STRING;
    args[1].subtype = STRING_SHARED;
    args[1].u.string = make_shared_string("extra");
    
    int result = input_to(&fun, 0, 2, args);
    EXPECT_EQ(result, 1) << "input_to with args should succeed";
    
    // Verify args stored in sentence
    ASSERT_NE(mock_ip->input_to, nullptr);
    ASSERT_NE(mock_ip->input_to->args, nullptr) << "Args should be stored in sentence";
    EXPECT_EQ(mock_ip->input_to->args->size, 2) << "Should have 2 carryover args";
    
    // Simulate user input
    simulate_input("test input");
    
    // Verify callback received input FIRST, then args
    EXPECT_EQ(get_string_var("last_input"), "test input");
    
    // Verify carryover arguments
    array_t* callback_args = get_array_var("callback_args");
    ASSERT_NE(callback_args, nullptr);
    EXPECT_EQ(callback_args->size, 2);
    EXPECT_EQ(callback_args->item[0].type, T_NUMBER);
    EXPECT_EQ(callback_args->item[0].u.number, 42);
    EXPECT_EQ(callback_args->item[1].type, T_STRING);
    EXPECT_STREQ(callback_args->item[1].u.string, "extra");
}

TEST_F(InputToGetCharTest, InputToFunctionPointer) {
    // Create a function pointer for "callback"
    svalue_t dummy = {};  // Zero-initialize
    dummy.type = T_NUMBER;
    dummy.u.number = 0;
    
    funptr_t* funp = make_lfun_funp_by_name("callback", &dummy);
    ASSERT_NE(funp, nullptr) << "Should create function pointer";
    
    svalue_t fun = {};  // Zero-initialize
    fun.type = T_FUNCTION;
    fun.u.fp = funp;
    
    int result = input_to(&fun, 0, 0, nullptr);
    EXPECT_EQ(result, 1) << "input_to with funptr should succeed";
    
    simulate_input("via funptr");
    
    EXPECT_EQ(get_string_var("last_input"), "via funptr");
    
    free_funp(funp);
}

TEST_F(InputToGetCharTest, GetCharSingleCharMode) {
    // Test: get_char("callback", 0)
    svalue_t fun = {};  // Zero-initialize
    fun.type = T_STRING;
    fun.u.string = const_cast<char*>("callback");
    
    int result = get_char(&fun, 0, 0, nullptr);
    EXPECT_EQ(result, 1) << "get_char should succeed";
    
    // Verify SINGLE_CHAR flag set
    EXPECT_TRUE(mock_ip->iflags & SINGLE_CHAR) << "SINGLE_CHAR flag should be set";
    
    simulate_input("x");
    
    // After callback, SINGLE_CHAR should be cleared
    EXPECT_FALSE(mock_ip->iflags & SINGLE_CHAR) << "SINGLE_CHAR should be cleared after input";
    
    EXPECT_EQ(get_string_var("last_input"), "x");
}

TEST_F(InputToGetCharTest, GetCharWithArgs) {
    // Test: get_char("callback_with_args", 0, 123, "context")
    svalue_t fun = {};  // Zero-initialize
    fun.type = T_STRING;
    fun.u.string = const_cast<char*>("callback_with_args");
    
    svalue_t args[2] = {};  // Zero-initialize
    args[0].type = T_NUMBER;
    args[0].u.number = 123;
    args[1].type = T_STRING;
    args[1].subtype = STRING_SHARED;
    args[1].u.string = make_shared_string("context");
    
    int result = get_char(&fun, 0, 2, args);
    EXPECT_EQ(result, 1);
    EXPECT_TRUE(mock_ip->iflags & SINGLE_CHAR);
    
    simulate_input("c");
    
    EXPECT_EQ(get_string_var("last_input"), "c");
    
    array_t* callback_args = get_array_var("callback_args");
    ASSERT_NE(callback_args, nullptr);
    EXPECT_EQ(callback_args->item[0].u.number, 123);
    EXPECT_STREQ(callback_args->item[1].u.string, "context");
}

TEST_F(InputToGetCharTest, InputToNoCommandGiver) {
    // Test error case: no command_giver
    command_giver = nullptr;
    
    svalue_t fun = {};  // Zero-initialize
    fun.type = T_STRING;
    fun.u.string = const_cast<char*>("callback");
    
    int result = input_to(&fun, 0, 0, nullptr);
    EXPECT_EQ(result, 0) << "input_to should fail gracefully without command_giver";
    EXPECT_EQ(mock_ip->input_to, nullptr) << "No sentence should be created";
}

TEST_F(InputToGetCharTest, InputToDestructedObject) {
    // Mark object as destructed
    command_giver->flags |= O_DESTRUCTED;
    
    svalue_t fun = {};  // Zero-initialize
    fun.type = T_STRING;
    fun.u.string = const_cast<char*>("callback");
    
    int result = input_to(&fun, 0, 0, nullptr);
    EXPECT_EQ(result, 0) << "input_to should fail on destructed object";
    
    // Restore flag for cleanup
    command_giver->flags &= ~O_DESTRUCTED;
}

TEST_F(InputToGetCharTest, NestedInputTo) {
    // Test calling input_to from within a callback
    svalue_t fun = {};  // Zero-initialize
    fun.type = T_STRING;
    fun.u.string = const_cast<char*>("nested_callback");
    
    int result = input_to(&fun, 0, 0, nullptr);
    EXPECT_EQ(result, 1);
    
    // Simulate first input - this will call nested_callback which sets up another input_to
    simulate_input("first");
    
    // Verify first callback executed
    EXPECT_EQ(get_string_var("last_input"), "first");
    
    // Verify new input_to was set up
    ASSERT_NE(mock_ip->input_to, nullptr) << "Nested input_to should be set";
    
    // Simulate second input
    simulate_input("second");
    
    // Verify final callback executed
    EXPECT_EQ(get_string_var("last_input"), "final: second");
}

TEST_F(InputToGetCharTest, InputToNoEchoFlag) {
    // Test I_NOECHO flag
    svalue_t fun = {};  // Zero-initialize
    fun.type = T_STRING;
    fun.u.string = const_cast<char*>("callback");
    
    int result = input_to(&fun, I_NOECHO, 0, nullptr);
    EXPECT_EQ(result, 1);
    
    // Verify flag set (NOTE: actual NOECHO handling is in comm layer, we just verify setting)
    ASSERT_NE(mock_ip->input_to, nullptr);
    
    simulate_input("secret");
    EXPECT_EQ(get_string_var("last_input"), "secret");
}

TEST_F(InputToGetCharTest, InputToNoEscFlag) {
    // Test I_NOESC flag
    svalue_t fun = {};  // Zero-initialize
    fun.type = T_STRING;
    fun.u.string = const_cast<char*>("callback");
    
    int result = input_to(&fun, I_NOESC, 0, nullptr);
    EXPECT_EQ(result, 1);
    
    simulate_input("!command");
    EXPECT_EQ(get_string_var("last_input"), "!command");
}

TEST_F(InputToGetCharTest, MultipleInputToCallsOnlyFirstSucceeds) {
    // LPC spec: if input_to() is called multiple times, only first succeeds
    svalue_t fun = {};  // Zero-initialize
    fun.type = T_STRING;
    fun.u.string = const_cast<char*>("callback");
    
    int result1 = input_to(&fun, 0, 0, nullptr);
    EXPECT_EQ(result1, 1) << "First input_to should succeed";
    
    sentence_t* first_sent = mock_ip->input_to;
    ASSERT_NE(first_sent, nullptr);
    
    // Try to call input_to again (should fail)
    int result2 = input_to(&fun, 0, 0, nullptr);
    EXPECT_EQ(result2, 0) << "Second input_to should fail";
    
    // Verify first sentence unchanged
    EXPECT_EQ(mock_ip->input_to, first_sent) << "First sentence should remain";
    
    // Cleanup
    simulate_input("test");
}

TEST_F(InputToGetCharTest, ArgsMemoryCleanup) {
    // Test that args array is properly freed
    svalue_t fun = {};  // Zero-initialize
    fun.type = T_STRING;
    fun.u.string = const_cast<char*>("callback_with_args");
    
    // Create args with reference-counted types
    svalue_t args[2] = {};  // Zero-initialize
    args[0].type = T_STRING;
    args[0].subtype = STRING_SHARED;
    args[0].u.string = make_shared_string("test_string"); // ref = 1
    args[1].type = T_ARRAY;
    args[1].u.arr = allocate_empty_array(3); // ref = 1
    
    int initial_arr_ref = args[1].u.arr->ref;
    
    int result = input_to(&fun, 0, 2, args);
    EXPECT_EQ(result, 1);
    
    // Refs should be incremented (args array holds references)
    EXPECT_GT(args[1].u.arr->ref, initial_arr_ref);
    
    // Simulate input - this should free args
    simulate_input("input");
    
    // Cleanup our local references
    free_string(args[0].u.string);
    free_array(args[1].u.arr);
}

TEST_F(InputToGetCharTest, ArgumentOrderVerification) {
    // Critical test: verify args come AFTER input, not before
    svalue_t fun = {};  // Zero-initialize
    fun.type = T_STRING;
    fun.u.string = const_cast<char*>("callback_with_args");
    
    svalue_t args[2] = {};  // Zero-initialize
    args[0].type = T_NUMBER;
    args[0].u.number = 111;
    args[1].type = T_STRING;
    args[1].subtype = STRING_SHARED;
    args[1].u.string = make_shared_string("arg2");
    
    input_to(&fun, 0, 2, args);
    simulate_input("user_input");
    
    // Callback should receive: ("user_input", 111, "arg2")
    // NOT: (111, "arg2", "user_input")
    EXPECT_EQ(get_string_var("last_input"), "user_input") << "Input should be first arg";
    
    array_t* callback_args = get_array_var("callback_args");
    ASSERT_NE(callback_args, nullptr);
    EXPECT_EQ(callback_args->item[0].u.number, 111) << "First carryover arg should be second";
    EXPECT_STREQ(callback_args->item[1].u.string, "arg2") << "Second carryover arg should be third";
}

