# tests/test_filepath/CMakeLists.txt

add_executable(test_filepath
    test_filepath.cpp
)

target_link_libraries(test_filepath PRIVATE misc GTest::gtest_main)

gtest_discover_tests(test_filepath
    PROPERTIES
        LABELS "MISC"
    DISCOVERY_TIMEOUT 20
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
