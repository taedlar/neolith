# unit-tests for I/O reactor
# Supports both POSIX (poll) and Windows (IOCP) implementations

add_executable(test_io_reactor
    test_io_reactor.cpp
)
target_link_libraries(test_io_reactor PRIVATE stem GTest::gtest_main)

gtest_discover_tests(test_io_reactor
    DISCOVERY_TIMEOUT 20
)
