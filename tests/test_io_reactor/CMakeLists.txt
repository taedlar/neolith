# unit-tests for I/O reactor

add_executable(test_io_reactor
    test_io_reactor.cpp
)
target_link_libraries(test_io_reactor PRIVATE GTest::gtest_main)
target_link_libraries(test_io_reactor PRIVATE port)

gtest_discover_tests(test_io_reactor
    DISCOVERY_TIMEOUT 20
)
