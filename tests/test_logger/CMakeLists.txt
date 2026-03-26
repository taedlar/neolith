# unit-tests for logger

add_executable(test_logger
    test_logger.cpp
)
target_link_libraries(test_logger PRIVATE GTest::gtest_main)
target_link_libraries(test_logger PRIVATE logger)

gtest_discover_tests(test_logger
    PROPERTIES
        LABELS "STEM"
    DISCOVERY_TIMEOUT 20
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
