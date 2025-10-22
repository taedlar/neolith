# tests/test_stralloc/CMakeLists.txt

add_executable(test_stralloc
    test_stralloc.cpp
)
target_link_libraries(test_stralloc PRIVATE stem GTest::gtest_main)

gtest_discover_tests(test_stralloc
    PROPERTIES
    LABELS "STEM"
)
