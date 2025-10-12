# CMakeLists.txt
cmake_minimum_required(VERSION 3.28)

# project configuration
project(neolith
	VERSION 2.0.0
	DESCRIPTION "A lightweight, embeddable scripting language"
	LANGUAGES C CXX
)
set(CMAKE_CXX_STANDARD 11)

if(MSVC)
	option(USE_STATIC_MSVC_RUNTIME "Enable static MSVC Runtime library (-MT/-MD)" ON)
	if (USE_STATIC_MSVC_RUNTIME)
		message(STATUS "Using static MSVC Runtime Library. (-MT)")
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	else()
		message(STATUS "Using dynamic-linking MSVC Runtime Library. (-MD)")
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
	endif()
    add_compile_options(/W3 /WX /permissive- /Zc:__cplusplus)
else()
    # add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# check for standard headers
include(CheckIncludeFile)
check_include_file("inttypes.h" HAVE_INTTYPES_H)
check_include_file("stdint.h" HAVE_STDINT_H)
check_include_file("unistd.h" HAVE_UNISTD_H)
check_include_file("limits.h" HAVE_LIMITS_H)

# check dependencies
find_package(BISON REQUIRED)

# generate global header "config.h" for build configurations
configure_file(config.h.in config.h @ONLY)
include_directories("${CMAKE_BINARY_DIR}")
add_compile_definitions(HAVE_CONFIG_H)

# add project components
add_subdirectory(lib/port)
add_subdirectory(lib/adt)
add_subdirectory(lib/efuns)
add_subdirectory(lib/lpc)
