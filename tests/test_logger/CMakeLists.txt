# unit-tests for logger

add_executable(test_logger
    test_logger.cpp
)
target_link_libraries(test_logger PRIVATE GTest::gtest_main)
target_link_libraries(test_logger PRIVATE logger)

gtest_discover_tests(test_logger)
