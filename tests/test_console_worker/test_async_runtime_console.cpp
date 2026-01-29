/**
 * @file test_async_runtime_console.cpp
 * @brief Tests for async_runtime console type detection functions
 */

#include <gtest/gtest.h>

extern "C" {
#include "async/async_runtime.h"
#include "async/console_worker.h"
}

/**
 * Test async_runtime_get_console_type returns NONE before add_console
 */
TEST(AsyncRuntimeConsole, GetConsoleTypeBeforeAdd) {
    async_runtime_t* runtime = async_runtime_init();
    ASSERT_NE(runtime, nullptr);
    
    /* Before calling add_console, type should be NONE */
    console_type_t type = async_runtime_get_console_type(runtime);
    EXPECT_EQ(type, CONSOLE_TYPE_NONE);
    
    async_runtime_deinit(runtime);
}

/**
 * Test async_runtime_add_console detects console type
 */
TEST(AsyncRuntimeConsole, AddConsoleDetectsType) {
    async_runtime_t* runtime = async_runtime_init();
    ASSERT_NE(runtime, nullptr);
    
    /* Call add_console to detect type */
    int result = async_runtime_add_console(runtime, nullptr);
    EXPECT_EQ(result, 0);
    
    /* Type should now be detected (not necessarily NONE) */
    console_type_t type = async_runtime_get_console_type(runtime);
    EXPECT_GE(type, CONSOLE_TYPE_NONE);
    EXPECT_LE(type, CONSOLE_TYPE_FILE);
    
    async_runtime_deinit(runtime);
}

/**
 * Test console type is consistent with console_detect_type()
 */
TEST(AsyncRuntimeConsole, ConsistentWithConsoleDetect) {
    async_runtime_t* runtime = async_runtime_init();
    ASSERT_NE(runtime, nullptr);
    
    /* Get type from console_worker detection */
    console_type_t worker_type = console_detect_type();
    
    /* Add console to runtime */
    ASSERT_EQ(async_runtime_add_console(runtime, nullptr), 0);
    
    /* Runtime should detect same type */
    console_type_t runtime_type = async_runtime_get_console_type(runtime);
    EXPECT_EQ(runtime_type, worker_type);
    
    async_runtime_deinit(runtime);
}

/**
 * Test async_runtime_get_console_type with NULL runtime
 */
TEST(AsyncRuntimeConsole, GetConsoleTypeNull) {
    console_type_t type = async_runtime_get_console_type(nullptr);
    EXPECT_EQ(type, CONSOLE_TYPE_NONE);
}

/**
 * Test async_runtime_add_console with NULL runtime
 */
TEST(AsyncRuntimeConsole, AddConsoleNull) {
    int result = async_runtime_add_console(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

/**
 * Test calling add_console multiple times
 */
TEST(AsyncRuntimeConsole, AddConsoleMultipleTimes) {
    async_runtime_t* runtime = async_runtime_init();
    ASSERT_NE(runtime, nullptr);
    
    /* First call */
    ASSERT_EQ(async_runtime_add_console(runtime, nullptr), 0);
    console_type_t type1 = async_runtime_get_console_type(runtime);
    
    /* Second call should succeed and maintain same type */
    ASSERT_EQ(async_runtime_add_console(runtime, nullptr), 0);
    console_type_t type2 = async_runtime_get_console_type(runtime);
    
    EXPECT_EQ(type1, type2);
    
    async_runtime_deinit(runtime);
}
