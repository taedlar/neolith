# CMakeLists.txt
cmake_minimum_required(VERSION 3.28)

include(CheckIncludeFile)

project(neolith)
set(CMAKE_CXX_STANDARD 11)

if(WIN32)
    add_compile_definitions(WIN32_WINNT=0x0601) # Windows 7 or higher
endif()

if(MSVC)
    add_compile_options(/W4 /WX /permissive- /Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# check for standard headers
check_include_file("inttypes.h" HAVE_INTTYPES_H)
check_include_file("stdint.h" HAVE_STDINT_H)
check_include_file("unistd.h" HAVE_UNISTD_H)
check_include_file("limits.h" HAVE_LIMITS_H)

# generate global header "config.h" for build configurations
configure_file(config.h.in config.h @ONLY)
include_directories("${CMAKE_BINARY_DIR}")
add_compile_definitions(HAVE_CONFIG_H)

add_subdirectory(lib/adt)
