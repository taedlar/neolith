#include <gtest/gtest.h>
extern "C" {
#include "logger/logger.h"
}
#include <filesystem>
#include <system_error>

using namespace testing;

// according to GoogleTest FAQ, the test suite name and test name should not
// contain underscores to avoid issues on some platforms.
// https://google.github.io/googletest/faq.html#why-should-test-suite-names-and-test-names-not-contain-underscore

TEST(LoggerTest, logMessage) {
    // Test that log_message returns a non-negative value when logging to stderr
    int result = log_message("", "Test log message: %d\n", 1);
    EXPECT_GE(result, 0);
    EXPECT_EQ(0, log_message("", "")); // empty message should return 0, no newline appended

    // Test that log_message returns a non-negative value when logging to current_log_file
    result = log_message(NULL, "Test log message to current log file: %d\n", 2);
    EXPECT_GE(result, 0);

    // Test that log_message returns a non-negative value when logging to a specific file
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::remove("test.log", ec); // ignore error if file does not exist
    result = log_message("test.log", "Test log message to file: %d\n", 3);
    EXPECT_GE(result, 0);
    EXPECT_TRUE(fs::exists("test.log"));
    fs::remove("test.log", ec); // clean up
}

TEST(LoggerTest, debugMessage) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::remove("debug_test.log", ec); // ignore error if file does not exist
    debug_set_log_file("debug_test.log");

    // Test that debug_message returns a non-negative value
    int result = debug_message("Test debug message: %s", "debug");
    EXPECT_GE(result, 0);
    EXPECT_GE(debug_message(""), 0); // a newline is added even for empty messages

    EXPECT_TRUE(fs::exists("debug_test.log"));
    fs::remove("debug_test.log", ec); // clean up
}

TEST(LoggerTest, debugSetLogWithDate) {
    // Enable date/time prepending
    debug_set_log_with_date(1);
    int result = debug_message("Test debug message with date: %s", "with date");
    EXPECT_GE(result, 0);

    // Disable date/time prepending
    debug_set_log_with_date(0);
    result = debug_message("Test debug message without date: %s", "without date");
    EXPECT_GE(result, 0);
}
