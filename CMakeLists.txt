# CMakeLists.txt
cmake_minimum_required(VERSION 3.28)

# project configuration
project(neolith
	VERSION 1.0.0
	DESCRIPTION "A minimalist LPMud server based on MudOS v22 and focus on stability and performance."
	LANGUAGES C CXX
)

include(CTest)
include(cmake/options.cmake) # options (cached)

set(CMAKE_CXX_STANDARD 17)

if(MSVC)
	if (USE_STATIC_MSVC_RUNTIME)
		message(STATUS "Using static MSVC Runtime Library. (-MT)")
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	else()
		message(STATUS "Using dynamic-linking MSVC Runtime Library. (-MD)")
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
	endif()
    # add_compile_options(/W3 /WX /permissive- /Zc:__cplusplus)
    add_compile_options(/W4 /wd4706 /permissive- /Zc:__cplusplus /EHs /utf-8)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
elseif(APPLE)
	add_compile_options(-Wall -Wextra -Wpedantic -Wno-strict-prototypes)
else()
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

# check for libraries
include(CheckLibraryExists)
if(UNIX)
    # check_library_exists(rt clock_gettime "" HAVE_LIBRT)
endif()

# check dependencies
find_package(BISON REQUIRED)
set(Boost_NO_WARN_NEW_VERSIONS ON) # Suppress warnings about newer Boost versions not known to FindBoost.cmake
# On MSVC, match the runtime linkage of the rest of the build so FindBoost selects
# the correct lib variant (static vs. DLL, /MT vs. /MD).  Without these hints the
# imported Boost::json target may resolve to the DLL import lib while Boost's
# autolink pragma tries to link the static-runtime variant (or vice-versa), causing
# LNK1104.  We also define BOOST_ALL_NO_LIB (in lib/efuns/CMakeLists.txt) to
# disable the autolink pragma entirely and let CMake's target link handle it.
if(MSVC)
    if(USE_STATIC_MSVC_RUNTIME)
        set(Boost_USE_STATIC_RUNTIME ON)
    else()
        set(Boost_USE_STATIC_RUNTIME OFF)
    endif()
endif()
if(POLICY CMP0167)
	cmake_policy(SET CMP0167 OLD)
endif()
find_package(Boost OPTIONAL_COMPONENTS json)
if(Boost_FOUND)
	set(HAVE_BOOST 1 CACHE BOOL "Boost libraries are available")
	message(STATUS "Found Boost: ${Boost_INCLUDE_DIRS} (found version ${Boost_VERSION})")
endif()

find_package(OpenSSL QUIET) # FindOpenSSL.cmake or FetchContent-provided
if(OPENSSL_FOUND)
	set(HAVE_OPENSSL 1 CACHE BOOL "OpenSSL library is available")
	message(STATUS "Found OpenSSL: ${OPENSSL_INCLUDE_DIR} (found version ${OPENSSL_VERSION})")
endif()
find_package(CURL QUIET) # FindCURL.cmake or FetchContent-provided
if(CURL_FOUND)
	set(HAVE_CURL 1 CACHE BOOL "libcurl library is available")
	message(STATUS "Found cURL: ${CURL_INCLUDE_DIRS} (found version ${CURL_VERSION_STRING})")
endif()
find_package(CARES QUIET) # FindCARES.cmake, c-ares config, pkg-config, or library lookup fallback
if(TARGET c-ares::cares)
	set(HAVE_CARES 1 CACHE BOOL "c-ares library is available")
	if(DEFINED CARES_VERSION)
		message(STATUS "Found c-ares: ${CARES_INCLUDE_DIR} (found version ${CARES_VERSION})")
	else()
		message(STATUS "Found c-ares: ${CARES_INCLUDE_DIR}")
	endif()
	add_compile_definitions(CARES_NO_DEPRECATED)
endif()

if(PACKAGE_JSON)
	if(NOT Boost_FOUND)
		message(STATUS "PACKAGE_JSON enabled: Boost not found; falling back to JsonCpp.")
		set(HAVE_JSONCPP 1 CACHE BOOL "JsonCpp library is available (fallback JSON parser)" FORCE)
		set(HAVE_BOOST_JSON 0 CACHE BOOL "Boost.JSON library is available" FORCE)
		set(BOOST_JSON_HEADER_ONLY 0 CACHE BOOL "Boost.JSON header-only mode is enabled" FORCE)
	else()
		if(Boost_json_FOUND)
			set(HAVE_BOOST_JSON 1 CACHE BOOL "Boost.JSON library is available" FORCE)
			set(BOOST_JSON_HEADER_ONLY 0 CACHE BOOL "Boost.JSON header-only mode is enabled" FORCE)
			message(STATUS "PACKAGE_JSON enabled: JSON efuns will use Boost.JSON (compiled library)")
		else()
			# Fall back to header-only mode (Boost.JSON supports header-only since Boost 1.75)
			find_file(BOOST_JSON_SRC_HPP NAMES boost/json/src.hpp
				HINTS ${Boost_INCLUDE_DIRS} NO_DEFAULT_PATH)
			if(NOT BOOST_JSON_SRC_HPP)
				message(STATUS "PACKAGE_JSON enabled: Boost.JSON unavailable (boost/json/src.hpp not found); falling back to JsonCpp.")
				set(HAVE_JSONCPP 1 CACHE BOOL "JsonCpp library is available (fallback JSON parser)" FORCE)
				set(HAVE_BOOST_JSON 0 CACHE BOOL "Boost.JSON library is available" FORCE)
				set(BOOST_JSON_HEADER_ONLY 0 CACHE BOOL "Boost.JSON header-only mode is enabled" FORCE)
			else()
				set(HAVE_BOOST_JSON 1 CACHE BOOL "Boost.JSON library is available" FORCE)
				set(BOOST_JSON_HEADER_ONLY 1 CACHE BOOL "Boost.JSON header-only mode is enabled" FORCE)
				message(STATUS "PACKAGE_JSON enabled: JSON efuns will use Boost.JSON (header-only)")
			endif()
		endif()
	endif()
endif()

if(PACKAGE_CURL)
	if(CURL_FOUND)
		message(STATUS "PACKAGE_CURL enabled: REST API efuns (perform_to, perform_using, in_perform) will be available")
	else()
		message(WARNING "PACKAGE_CURL requested but libcurl was not found; disabling PACKAGE_CURL.")
		set(PACKAGE_CURL OFF CACHE BOOL "Enable REST API efuns (requires libcurl)" FORCE)
	endif()
endif()

# unit-tests
if(NEOLITH_BUILD_TESTS)
	find_package(GTest QUIET) # Always do FetchContent_MakeAvailable() to pin the version of GTest we use.
	if(TARGET GTest::gtest OR TARGET GTest::gtest_main)
		set(HAVE_GTEST 1 CACHE BOOL "GoogleTest framework is available" FORCE)
		message(STATUS "Found GoogleTest: ${GTest_INCLUDE_DIRS} (found version ${GTest_VERSION})")
		if(GTest_INCLUDE_DIRS)
			include_directories(${GTest_INCLUDE_DIRS}) # for white-box tests that needs <gtest/gtest_prod.h> to access private members of stem
		endif()
	else()
		set(HAVE_GTEST 0 CACHE BOOL "GoogleTest framework is available" FORCE)
		set(NEOLITH_BUILD_TESTS OFF CACHE BOOL "Build Neolith unit tests (requires BUILD_TESTING and GoogleTest)" FORCE)
		message(STATUS "GoogleTest not found: disabling NEOLITH_BUILD_TESTS.")
	endif()
else()
	set(HAVE_GTEST 0 CACHE BOOL "GoogleTest framework is available" FORCE)
endif()

# generate global header "config.h" for build configurations
configure_file(config.h.in config.h @ONLY)
include_directories(
	${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/lib
	${CMAKE_BINARY_DIR}
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
	enable_testing()
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
