# tests/test_efuns/CMakeLists.txt

add_executable(test_efuns
    test_efuns.cpp
)
target_link_libraries(test_efuns PRIVATE stem GTest::gtest_main)

# setup testing data
# (the default working directory is ${CMAKE_CURRENT_BINARY_DIR} when running tests)
add_custom_command(TARGET test_efuns POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples ${CMAKE_CURRENT_BINARY_DIR}
)

gtest_discover_tests(test_efuns
    PROPERTIES
    LABELS "LPC"
)
