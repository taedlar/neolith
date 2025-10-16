# CMakeLists.txt
cmake_minimum_required(VERSION 3.28)

# project configuration
project(neolith
	VERSION 1.0.0
	DESCRIPTION "A minimalist LPMud server based on MudOS v22 and focus on stability and performance."
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
    # add_compile_options(/W3 /WX /permissive- /Zc:__cplusplus)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus)
else()
    # add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    add_compile_options(-Wall -Wextra -Wpedantic)
	add_compile_definitions(_GNU_SOURCE)
endif()

# check for standard headers
include(CheckIncludeFile)
check_include_file(argp.h HAVE_ARGP_H)
check_include_file(fcntl.h HAVE_FCNTL_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(limits.h HAVE_LIMITS_H)
check_include_file(signal.h HAVE_SIGNAL_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(sys/param.h HAVE_SYS_PARAM_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(sys/wait.h HAVE_SYS_WAIT_H)
check_include_file(unistd.h HAVE_UNISTD_H)

# check dependencies
find_package(BISON REQUIRED)

# generate global header "config.h" for build configurations
configure_file(config.h.in config.h @ONLY)
include_directories("${CMAKE_BINARY_DIR}")
add_compile_definitions(HAVE_CONFIG_H)

# add project components
add_subdirectory(lib/port)
add_subdirectory(lib/logger)
add_subdirectory(lib/misc)
add_subdirectory(lib/efuns)
add_subdirectory(lib/lpc)
add_subdirectory(lib/socket)
add_subdirectory(src)
