# Test async_worker

add_executable(test_async_worker
    test_async_worker_main.cpp
    test_async_worker_lifecycle.cpp
)

target_link_libraries(test_async_worker PRIVATE
    async
    port
    GTest::gtest
    GTest::gtest_main
)

# Unix needs pthread for threading
if(UNIX)
    target_link_libraries(test_async_worker PRIVATE pthread)
endif()

gtest_discover_tests(test_async_worker
    DISCOVERY_TIMEOUT 20
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
