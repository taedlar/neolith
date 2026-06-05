# Test async_queue

add_executable(test_async_queue
    test_async_queue.cpp
    test_async_queue_threadsafety.cpp
)

target_link_libraries(test_async_queue PRIVATE async port GTest::gtest_main)
target_compile_definitions(test_async_queue PRIVATE NO_STEM)

# Windows needs ws2_32 for WSAStartup/WSACleanup
if(WIN32)
    target_link_libraries(test_async_queue PRIVATE ws2_32)
endif()

gtest_discover_tests(test_async_queue
    PROPERTIES
        LABELS "STEM"
    DISCOVERY_TIMEOUT 20
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
