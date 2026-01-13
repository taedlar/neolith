# tests/test_lpc_interpreter/CMakeLists.txt

add_executable(test_lpc_interpreter
    test_double_precision.cpp
    test_int64.cpp
    test_lpc_interpreter.cpp
)

target_link_libraries(test_lpc_interpreter PRIVATE stem GTest::gtest_main)

# setup testing data
# (the default working directory is ${CMAKE_CURRENT_BINARY_DIR} when running tests)
add_custom_command(TARGET test_lpc_interpreter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/m3_mudlib ${CMAKE_CURRENT_BINARY_DIR}/m3_mudlib
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/examples/m3.conf ${CMAKE_CURRENT_BINARY_DIR}
)

gtest_discover_tests(test_lpc_interpreter
    PROPERTIES
    LABELS "LPC"
    DISCOVERY_TIMEOUT 20
)

