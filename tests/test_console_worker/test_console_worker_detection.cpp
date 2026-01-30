/**
 * @file test_console_worker_detection.cpp
 * @brief Tests for console type detection
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <gtest/gtest.h>

extern "C" {
#include "async/console_worker.h"
}

/**
 * Test console type detection
 */
TEST(ConsoleWorkerDetection, ConsoleTypeStr) {
    EXPECT_STREQ("NONE", console_type_str(CONSOLE_TYPE_NONE));
    EXPECT_STREQ("REAL", console_type_str(CONSOLE_TYPE_REAL));
    EXPECT_STREQ("PIPE", console_type_str(CONSOLE_TYPE_PIPE));
    EXPECT_STREQ("FILE", console_type_str(CONSOLE_TYPE_FILE));
}

/**
 * Test console detection (basic sanity check)
 * 
 * This test depends on how the test is run:
 * - Interactive console: CONSOLE_TYPE_REAL
 * - CTest runner: CONSOLE_TYPE_PIPE or CONSOLE_TYPE_NONE
 * - Redirected stdin: CONSOLE_TYPE_FILE
 */
TEST(ConsoleWorkerDetection, DetectType) {
    console_type_t type = console_detect_type();
    
    /* Valid types */
    EXPECT_GE(type, CONSOLE_TYPE_NONE);
    EXPECT_LE(type, CONSOLE_TYPE_FILE);
    
    /* Type should be consistent across multiple calls */
    EXPECT_EQ(type, console_detect_type());
}
