# tests/test_stack_machine/CMakeLists.txt

add_executable(test_stack_machine
    test_object_hash.cpp
    test_stack_machine.cpp
)

target_link_libraries(test_stack_machine PRIVATE stem GTest::gtest_main)

# setup testing data
# (the default working directory is ${CMAKE_CURRENT_BINARY_DIR} when running tests)
add_custom_command(TARGET test_stack_machine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples ${CMAKE_CURRENT_BINARY_DIR}
)

gtest_discover_tests(test_stack_machine
    PROPERTIES
    LABELS "STEM"
)
