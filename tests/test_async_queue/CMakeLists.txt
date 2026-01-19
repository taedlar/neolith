# Test async_queue

add_executable(test_async_queue
    test_async_queue_main.cpp
    test_async_queue_basic.cpp
    test_async_queue_threadsafety.cpp
)

target_link_libraries(test_async_queue PRIVATE
    async
    port
    GTest::gtest
    GTest::gtest_main
)

# Windows needs ws2_32 for WSAStartup/WSACleanup
if(WIN32)
    target_link_libraries(test_async_queue PRIVATE ws2_32)
endif()

gtest_discover_tests(test_async_queue
    DISCOVERY_TIMEOUT 20
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
