# tests/test_socket_efuns/CMakeLists.txt

add_executable(test_socket_efuns
    test_socket_efuns_behavior.cpp
    test_socket_efuns_resolver.cpp
    fixtures.hpp
)

target_link_libraries(test_socket_efuns PRIVATE stem GTest::gtest_main)

# setup testing data
# (the default working directory is ${CMAKE_CURRENT_BINARY_DIR} when running tests)
add_custom_command(TARGET test_socket_efuns POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples/m3_mudlib ${CMAKE_CURRENT_BINARY_DIR}/m3_mudlib
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/examples/m3.conf ${CMAKE_CURRENT_BINARY_DIR}
)

gtest_discover_tests(test_socket_efuns
    PROPERTIES
    LABELS "STEM;SOCKET"
    DISCOVERY_TIMEOUT 20
)
