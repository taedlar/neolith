# tests/test_lpc_compiler/CMakeLists.txt

add_executable(test_lpc_compiler
    test_lpc_compiler.cpp
)
target_link_libraries(test_lpc_compiler PRIVATE stem GTest::gtest_main)
