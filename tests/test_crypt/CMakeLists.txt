# tests/test_crypt/CMakeLists.txt

add_executable(test_crypt test_crypt.cpp)
target_link_libraries(test_crypt PRIVATE GTest::gtest_main port)

gtest_discover_tests(test_crypt
    PROPERTIES
        LABELS "EFUNS"
    DISCOVERY_TIMEOUT 20
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
