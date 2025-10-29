# tests/test_lpc_lexer/CMakeLists.txt

add_executable(test_lpc_lexer
    test_lpc_lexer.cpp
)
target_link_libraries(test_lpc_lexer PRIVATE stem GTest::gtest_main)

# setup testing data
# (the default working directory is ${CMAKE_CURRENT_BINARY_DIR} when running tests)
add_custom_command(TARGET test_lpc_lexer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples ${CMAKE_CURRENT_BINARY_DIR}
)

gtest_discover_tests(test_lpc_lexer
    PROPERTIES
    LABELS "LPC"
)
