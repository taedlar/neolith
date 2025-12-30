# unit-tests for I/O reactor
# Note: Currently only supported on POSIX platforms (Linux, macOS, BSD)
# Windows IOCP implementation is TODO

if(NOT WIN32)
    add_executable(test_io_reactor
        test_io_reactor.cpp
    )
    target_link_libraries(test_io_reactor PRIVATE stem GTest::gtest_main)

    gtest_discover_tests(test_io_reactor
        DISCOVERY_TIMEOUT 20
    )
endif()
