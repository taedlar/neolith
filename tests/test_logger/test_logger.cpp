#include <gtest/gtest.h>
extern "C" {
#include "logger/logger.h"
}

using namespace testing;

// according to GoogleTest FAQ, the test suite name and test name should not
// contain underscores to avoid issues on some platforms.
// https://google.github.io/googletest/faq.html#why-should-test-suite-names-and-test-names-not-contain-underscore

TEST(LoggerTest, logMessage)
{
    // Test that log_message returns a non-negative value when logging to stderr
    int result = log_message("", "Test log message: %d", 1);
    EXPECT_GE(result, 0);

    // Test that log_message returns a non-negative value when logging to current_log_file
    result = log_message(NULL, "Test log message to current log file: %d", 2);
    EXPECT_GE(result, 0);
}
