cmake_minimum_required(VERSION 3.21)

# Console Worker Unit Tests
project(test_console_worker)

# Test executable
add_executable(test_console_worker
    test_console_worker_lifecycle.cpp
    test_console_worker_detection.cpp
    test_async_runtime_console.cpp
)

# Link to async library (includes console_worker) and logger
target_link_libraries(test_console_worker PRIVATE
    async
    logger
    GTest::gtest_main
)

# Windows needs ws2_32 for socket functions
if(WIN32)
    target_link_libraries(test_console_worker PRIVATE ws2_32)
endif()

# Test discovery
gtest_discover_tests(test_console_worker
    PROPERTIES
        DISCOVERY_TIMEOUT 20
)
