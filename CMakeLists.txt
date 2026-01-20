# CMakeLists.txt
cmake_minimum_required(VERSION 3.28)

# project configuration
project(neolith
	VERSION 1.0.0
	DESCRIPTION "A minimalist LPMud server based on MudOS v22 and focus on stability and performance."
	LANGUAGES C CXX
)
set(CMAKE_CXX_STANDARD 17)

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
    add_compile_options(/W4 /wd4706 /permissive- /Zc:__cplusplus /utf-8)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    # add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    add_compile_options(-Wall -Wextra -Wpedantic)
	add_compile_definitions(_GNU_SOURCE)
endif()

# check for standard headers
include(CheckIncludeFile)
check_include_file(argp.h HAVE_ARGP_H)
check_include_file(dirent.h HAVE_DIRENT_H)
check_include_file(fcntl.h HAVE_FCNTL_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(limits.h HAVE_LIMITS_H)
check_include_file(netdb.h HAVE_NETDB_H)
check_include_file(signal.h HAVE_SIGNAL_H)
check_include_file(stdarg.h HAVE_STDARG_H)
check_include_file(stddef.h HAVE_STDDEF_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(sys/ioctl.h HAVE_SYS_IOCTL_H)
check_include_file(sys/param.h HAVE_SYS_PARAM_H)
check_include_file(sys/resource.h HAVE_SYS_RESOURCE_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(sys/wait.h HAVE_SYS_WAIT_H)
check_include_file(termios.h HAVE_TERMIOS_H)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(arpa/telnet.h HAVE_ARPA_TELNET_H)

# check for standard functions
include(CheckSymbolExists)
check_symbol_exists(gettimeofday sys/time.h HAVE_GETTIMEOFDAY)
check_symbol_exists(poll poll.h HAVE_POLL)
check_symbol_exists(realpath stdlib.h HAVE_REALPATH)
check_symbol_exists(stpcpy string.h HAVE_STPCPY)
check_symbol_exists(stpncpy string.h HAVE_STPNCPY)
check_symbol_exists(strtod stdlib.h HAVE_STRTOD)

# check for libraries
include(CheckLibraryExists)
if(UNIX)
    check_library_exists(rt clock_gettime "" HAVE_LIBRT)
endif()

# Set timer support flags
if(WIN32)
    set(HAVE_WIN32_TIMER TRUE)
elseif(HAVE_LIBRT)
    set(HAVE_POSIX_TIMER TRUE)
else()
    set(HAVE_FALLBACK_TIMER TRUE)
endif()

# check dependencies
find_package(BISON REQUIRED)

# generate global header "config.h" for build configurations
configure_file(config.h.in config.h @ONLY)
include_directories("${CMAKE_BINARY_DIR}")
add_compile_definitions(HAVE_CONFIG_H)

# add project components
add_subdirectory(lib/port)
add_subdirectory(lib/async)
add_subdirectory(lib/logger)
add_subdirectory(lib/rc)
add_subdirectory(lib/misc)
add_subdirectory(lib/efuns)
add_subdirectory(lib/lpc)
add_subdirectory(lib/socket)
add_subdirectory(src)

# unit-tests
include(CTest)
if(BUILD_TESTING)
	enable_testing()
	find_package(GTest)
	if (GTest_FOUND)
		include(GoogleTest)
		message(STATUS "Found GoogleTest")
		add_subdirectory(tests/test_async_queue)
		add_subdirectory(tests/test_async_worker)
		add_subdirectory(tests/test_console_worker)
		add_subdirectory(tests/test_backend)
		add_subdirectory(tests/test_efuns)
		add_subdirectory(tests/test_logger)
		add_subdirectory(tests/test_lpc_compiler)
		add_subdirectory(tests/test_lpc_interpreter)
		add_subdirectory(tests/test_lpc_lexer)
		add_subdirectory(tests/test_simul_efuns)
		add_subdirectory(tests/test_stack_machine)
		add_subdirectory(tests/test_stralloc)
	else()
		message(WARNING "GoogleTest not found; unit tests will not be built.")
	endif()
endif()
