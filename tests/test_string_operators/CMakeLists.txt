add_executable(test_string_operators
    test_string_operators_main.cpp
    test_string_operators_lpc.cpp
)

target_link_libraries(test_string_operators PRIVATE stem GTest::gtest_main)

gtest_discover_tests(test_string_operators
    PROPERTIES
        LABELS "STEM"
    DISCOVERY_TIMEOUT 20
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_command(TARGET test_string_operators POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/examples/m3_mudlib
        ${CMAKE_CURRENT_BINARY_DIR}/m3_mudlib
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/examples/m3.conf
        ${CMAKE_CURRENT_BINARY_DIR}
)
