# CMakeLists.txt
cmake_minimum_required(VERSION 3.28)

# =====================
# project configuration
# =====================
project(neolith
	VERSION 1.0.0
	DESCRIPTION "A LPMud server (MudOS fork) with modern build system and focus on supporting tool-use for developers."
	LANGUAGES C CXX
)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
include(CTest) # calls enable_testing() if BUILD_TESTING is ON
include(options) # defines options

if(MSVC)
	# Visual Studio 2019 or later
    add_compile_options(/W4 /wd4706 /permissive- /Zc:__cplusplus /EHs /utf-8 /nologo)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	# GCC and Clang
	add_compile_options(-Wall -Wextra -Wpedantic "$<$<COMPILE_LANGUAGE:C>:-Wno-strict-prototypes>")
endif()

if(APPLE)
	set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "Build architectures for Mac OS X")
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

# ==================
# check dependencies
# ==================
include(utils)
include(FetchContent)
FetchContent_Declare(
	GoogleTest
	GIT_REPOSITORY https://github.com/google/googletest.git
	GIT_TAG v1.17.0
	EXCLUDE_FROM_ALL
)
find_package(BISON REQUIRED)
if(NEOLITH_USE_BOOST)
	# set(Boost_VERBOSE ON)
	set(Boost_USE_MULTITHREADED ON)
	find_boost(1.70 COMPONENTS json)
	find_boost_headers(boost/json.hpp)
endif()
find_package(OpenSSL)
find_package(CURL)
find_package(CARES)

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
	if(HAVE_BOOST)
		if(HAVE_BOOST_JSON_HPP)
			if(TARGET Boost::json)
				message(STATUS "PACKAGE_JSON enabled: using Boost.JSON (compiled library).")
			else()
				message(STATUS "PACKAGE_JSON enabled: using Boost.JSON (header-only).")
			endif()
		else()
			message(STATUS "PACKAGE_JSON enabled: using JsonCpp (Boost too old).")
		endif()
	else()
		message(STATUS "PACKAGE_JSON enabled: using JsonCpp.")
		set(HAVE_JSONCPP TRUE)
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
	# always fetch GoogleTest as recommended by GoogleTest maintainers
	set(INSTALL_GTEST OFF CACHE BOOL "Disable installation of GoogleTest" FORCE)
	FetchContent_MakeAvailable(GoogleTest)
	if(TARGET GTest::gtest OR TARGET GTest::gtest_main)
		set(HAVE_GTEST TRUE)
		link_libraries(GTest::gtest) # make FRIEND_TEST() macro available to all targets
		message(STATUS "GoogleTest found: ${googletest_BINARY_DIR}")
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
add_subdirectory(lib/logger)
add_subdirectory(lib/rc)
add_subdirectory(lib/misc)
add_subdirectory(lib/efuns)
add_subdirectory(lib/lpc)
add_subdirectory(lib/socket)
add_subdirectory(lib/curl)
if(HAVE_JSONCPP)
	add_subdirectory(lib/json)
endif()
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
