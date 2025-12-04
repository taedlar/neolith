# tests/test_lpc_compiler/CMakeLists.txt

add_executable(test_lpc_compiler
    test_lpc_compiler.cpp
    test_save_binary.cpp
    fixtures.hpp
)
target_link_libraries(test_lpc_compiler PRIVATE stem GTest::gtest_main)

# setup testing data
# (the default working directory is ${CMAKE_CURRENT_BINARY_DIR} when running tests)
add_custom_command(TARGET test_lpc_compiler POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples ${CMAKE_CURRENT_BINARY_DIR}
)

gtest_discover_tests(test_lpc_compiler
    PROPERTIES
    LABELS "LPC"
    DISCOVERY_TIMEOUT 20
)
