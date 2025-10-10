# CMakeLists.txt
cmake_minimum_required(VERSION 3.28)

project(neolith)

set(CMAKE_CXX_STANDARD 11)

find_package(GTest CONFIG REQUIRED)
# target_link_libraries(main PRIVATE GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)

find_package(spdlog CONFIG REQUIRED)
# target_link_libraries(main PRIVATE spdlog::spdlog_header_only)
