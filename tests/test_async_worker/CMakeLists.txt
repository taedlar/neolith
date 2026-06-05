# Test async_worker

add_executable(test_async_worker
    test_async_worker.cpp
)

target_link_libraries(test_async_worker PRIVATE async port GTest::gtest_main)
target_compile_definitions(test_async_worker PRIVATE NO_STEM)

# Unix needs pthread for threading
if(UNIX)
    target_link_libraries(test_async_worker PRIVATE pthread)
endif()

gtest_discover_tests(test_async_worker
    PROPERTIES
        LABELS "STEM"
    DISCOVERY_TIMEOUT 20
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
