# tests/test_simul_efun/CMakeLists.txt

add_executable(test_simul_efuns
    test_simul_efuns.cpp
)
target_link_libraries(test_simul_efuns PRIVATE stem GTest::gtest_main)

# setup testing data
# (the default working directory is ${CMAKE_CURRENT_BINARY_DIR} when running tests)
add_custom_command(TARGET test_simul_efuns POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/examples ${CMAKE_CURRENT_BINARY_DIR}
)

gtest_discover_tests(test_simul_efuns
    PROPERTIES
    LABELS "SimulEfun"
    DISCOVERY_TIMEOUT 20
)
