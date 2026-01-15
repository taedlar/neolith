# tests/test_backend/CMakeLists.txt

add_executable(test_backend
    test_backend.cpp
    test_backend_timer.cpp
    test_command_fairness.cpp
)

target_link_libraries(test_backend PRIVATE stem GTest::gtest_main)

# setup testing data
# (the default working directory is ${CMAKE_CURRENT_BINARY_DIR} when running tests)
add_custom_command(TARGET test_backend POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/m3_mudlib ${CMAKE_CURRENT_BINARY_DIR}/m3_mudlib
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/examples/m3.conf ${CMAKE_CURRENT_BINARY_DIR}
)

gtest_discover_tests(test_backend
    PROPERTIES
    LABELS "STEM"
    DISCOVERY_TIMEOUT 20
)
