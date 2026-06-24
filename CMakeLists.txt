# CMakeLists.txt
cmake_minimum_required(VERSION 3.28)

# project configuration
project(neolith
	VERSION 1.0.0
	DESCRIPTION "A LPMud server (MudOS fork) with modern build system and focus on supporting tool-use for developers."
	LANGUAGES C CXX
)

include(CTest) # option: BUILD_TESTING
include(cmake/options.cmake) # project options

set(CMAKE_CXX_STANDARD 17)

if(MSVC)
	# Visual Studio 2019 or later
    add_compile_options(/W4 /wd4706 /permissive- /Zc:__cplusplus /EHs /utf-8 /nologo)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
elseif(UNIX)
	# GCC and Clang
	add_compile_options(-Wall -Wextra -Wpedantic "$<$<COMPILE_LANGUAGE:C>:-Wno-strict-prototypes>")
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
check_include_file(stdbool.h HAVE_STDBOOL_H)
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
check_symbol_exists(strtod stdlib.h HAVE_STRTOD)

# check for standard libraries
include(CheckLibraryExists)
check_library_exists(m sqrt "" HAVE_LIBM)

# check dependencies
include(FetchContent)
FetchContent_Declare(
	GoogleTest
	GIT_REPOSITORY https://github.com/google/googletest.git
	GIT_TAG v1.17.0
	EXCLUDE_FROM_ALL
)
find_package(BISON REQUIRED)
set(Boost_NO_WARN_NEW_VERSIONS ON)
set(Boost_USE_STATIC_RUNTIME OFF)
set(Boost_COMPILER "-vc142")
find_package(Boost 1.71 CONFIG COMPONENTS json)
find_package(OpenSSL)
find_package(CURL)
include(cmake/FindCARES.cmake)

if(Boost_FOUND AND TARGET Boost::boost)
	set(HAVE_BOOST TRUE)
	message(STATUS "Boost found: portable C++ features will be used")
endif()

if(OPENSSL_FOUND AND TARGET OpenSSL::Crypto)
	set(HAVE_OPENSSL TRUE)
	message(STATUS "OpenSSL found: advanced cryptography efuns will be available")
endif()

if(CARES_FOUND AND TARGET c-ares::cares)
	set(HAVE_CARES TRUE)
	message(STATUS "c-ares found: asynchronous DNS resolution will be available")
endif()

if(PACKAGE_SOCKETS)
	message(STATUS "PACKAGE_SOCKETS enabled: socket efuns will be available")
endif()

if(PACKAGE_JSON)
	if(Boost_FOUND AND TARGET Boost::boost)
		# Boost.JSON is faster than JsonCpp and is the preferred JSON library for neolith.
		if(Boost_json_FOUND AND TARGET Boost::json)
			# Boost.JSON is available as a compiled library (Boost 1.81+)
			set(HAVE_BOOST_JSON TRUE)
			set(BOOST_JSON_HEADER_ONLY FALSE)
			message(STATUS "PACKAGE_JSON enabled: JSON efuns will use Boost.JSON (fast, compiled library)")
		else()
			# Fall back to header-only mode (Boost.JSON supports header-only since Boost 1.75)
			set(HAVE_BOOST_JSON TRUE)
			set(BOOST_JSON_HEADER_ONLY TRUE)
			message(STATUS "PACKAGE_JSON enabled: JSON efuns will use Boost.JSON (header-only)")
		endif()
	else()
		# Fall back to JsonCpp if Boost is not found
		set(HAVE_BOOST_JSON FALSE)
		set(BOOST_JSON_HEADER_ONLY FALSE)
		set(HAVE_JSONCPP TRUE)
		message(STATUS "PACKAGE_JSON enabled: JSON efuns will use JsonCpp.")
	endif()
endif()

if(PACKAGE_CURL)
	if (CURL_FOUND)
		set(HAVE_CURL TRUE)
		message(STATUS "PACKAGE_CURL enabled: cURL efuns (perform_to, perform_using, in_perform) will be available")
	else()
		set(PACKAGE_CURL FALSE CACHE BOOL "Enable curl efuns (perform_to, perform_using, in_perform)" FORCE)
		message(WARNING "PACKAGE_CURL disabled: unable to find cURL library. Install cURL development package, specify FETCH_CURL_FROM_SOURCE, or disable PACKAGE_CURL. Re-run CMake configure with --fresh to apply changes.")
	endif()
endif()

# unit-tests
if(NEOLITH_BUILD_TESTS)
	FetchContent_MakeAvailable(GoogleTest)
	if(TARGET GTest::gtest OR TARGET GTest::gtest_main)
		set(HAVE_GTEST TRUE)
		link_libraries(GTest::gtest) # for FRIEND_TEST() macro
		message(STATUS "GoogleTest found: unit-tests will be enabled")
	else()
		set(NEOLITH_BUILD_TESTS FALSE CACHE BOOL "Enable unit-tests" FORCE)
		message(WARNING "GoogleTest not found: unit-tests will be disabled. Check your internet connection, or disable tests. Re-run CMake configure with --fresh to apply changes.")
	endif()
endif()

# generate global header "config.h" for build configurations
configure_file(config.h.in config.h @ONLY)
include_directories(
	${CMAKE_SOURCE_DIR}	# for src/std.h
	${CMAKE_BINARY_DIR} # for generated headers
)
add_compile_definitions(HAVE_CONFIG_H)

# add project components
add_subdirectory(lib/port)
add_subdirectory(lib/async)
add_subdirectory(lib/json)
add_subdirectory(lib/logger)
add_subdirectory(lib/rc)
add_subdirectory(lib/misc)
add_subdirectory(lib/efuns)
add_subdirectory(lib/lpc)
add_subdirectory(lib/socket)
add_subdirectory(lib/curl)
add_subdirectory(src)

if(NEOLITH_BUILD_TESTS)
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
	add_subdirectory(tests/test_string_operators)
	add_subdirectory(tests/test_filepath)
endif()
