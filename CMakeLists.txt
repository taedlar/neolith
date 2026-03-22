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
    # check_library_exists(rt clock_gettime "" HAVE_LIBRT)
endif()

# check dependencies
find_package(BISON REQUIRED)
find_package(Boost)
if(Boost_FOUND)
	set(HAVE_BOOST 1)
	message(STATUS "Found Boost: ${Boost_INCLUDE_DIRS} (found version ${Boost_VERSION})")
endif()

find_package(OpenSSL QUIET) # FindOpenSSL.cmake or FetchContent-provided
if(OPENSSL_FOUND)
	set(HAVE_OPENSSL 1)
	message(STATUS "Found OpenSSL: ${OPENSSL_INCLUDE_DIR} (found version ${OPENSSL_VERSION})")
endif()
find_package(CURL QUIET) # FindCURL.cmake or FetchContent-provided
if(CURL_FOUND)
	set(HAVE_CURL 1)
	message(STATUS "Found cURL: ${CURL_INCLUDE_DIRS} (found version ${CURL_VERSION_STRING})")
endif()
find_package(CARES QUIET) # FindCARES.cmake, c-ares config, pkg-config, or library lookup fallback
if(TARGET c-ares::cares)
	set(HAVE_CARES 1)
	if(DEFINED CARES_VERSION)
		message(STATUS "Found c-ares: ${CARES_VERSION}")
	else()
		message(STATUS "Found c-ares: ${CARES_LIBRARY}")
	endif()
endif()

# package options (configurable at CMake time)
option(ENABLE_SOCKET_CONNECT_DNS "Enable built-in async DNS hostname resolution for socket_connect()" OFF)
if(ENABLE_SOCKET_CONNECT_DNS)
	set(PACKAGE_SOCKET_CONNECT_DNS 1)
	message(STATUS "Built-in socket_connect() hostname DNS enabled")
endif()

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
	find_package(GTest QUIET) # FindGTest.cmake or FetchContent-provided
	if (GTest_FOUND)
		message(STATUS "Found GoogleTest: ${GTest_INCLUDE_DIRS} (found version ${GTest_VERSION})")
		include(GoogleTest)
		add_subdirectory(tests/test_async_queue)
		add_subdirectory(tests/test_async_worker)
		add_subdirectory(tests/test_console_worker)
		add_subdirectory(tests/test_backend)
		add_subdirectory(tests/test_crypt)
		add_subdirectory(tests/test_efuns)
		add_subdirectory(tests/test_socket_efuns)
		add_subdirectory(tests/test_logger)
		add_subdirectory(tests/test_lpc_compiler)
		add_subdirectory(tests/test_lpc_interpreter)
		add_subdirectory(tests/test_lpc_lexer)
		add_subdirectory(tests/test_simul_efuns)
		add_subdirectory(tests/test_stack_machine)
		add_subdirectory(tests/test_stralloc)
	else()
		message(STATUS "GoogleTest not found: ***** Unit-tests will not be built.")
		message(STATUS "Disable BUILD_TESTING or configure FETCH_GOOGLETEST_FROM_SOURCE to suppress this message.")
	endif()
endif()
