# prebuild-openssl.cmake - CMake script to pre-build OpenSSL from source code.
#
# This script is intended to be included in the main CMakeLists.txt before the
# find_package(OpenSSL) call. It checks for the existence of the OpenSSL
# library, and if not found, it will download the OpenSSL source code, build it
# and install it to a specified location. The script also checks for necessary
# build tools (Perl, Make/NMake, NASM) and dependencies before attempting to
# build OpenSSL.
#
# The script is designed to be compatible with CMake's FindOpenSSL module, which
# will look for the OpenSSL installation in the specified OPENSSL_ROOT_DIR.
#
# Usage:
#   # declare source code location of `OpenSSL` via `FetchContent_Declare`
#   set(OPENSSL_ROOT_DIR "/path/to/prebuilt/openssl")
#   include(cmake/prebuild-openssl.cmake)
#
# Note:
# - If the directory specified by OPENSSL_ROOT_DIR already exists, the script will
#   skip the build step (assuming OpenSSL is already built and installed there).
# - To force a rebuild, remove the directory specified by OPENSSL_ROOT_DIR before
#   re-configuring the main project.
#
# References:
# - OpenSSL build documentation: https://github.com/openssl/openssl/blob/master/INSTALL.md

cmake_minimum_required(VERSION 3.28)

message(CHECK_START "Checking OPENSSL_ROOT_DIR")
if (EXISTS ${OPENSSL_ROOT_DIR})
    # OPENSSL_ROOT_DIR already exists, assume OpenSSL is already built and installed
	# Proceed to import the pre-built OpenSSL library in the main CMakeLists.txt
	# via find_package(OpenSSL)
    message(CHECK_PASS "${OPENSSL_ROOT_DIR} exists - skipping prebuild.")
    return()
else()
    # OPENSSL_ROOT_DIR does not exist, attempt to pre-build OpenSSL
    message(CHECK_FAIL "not found: attempting to pre-build OpenSSL")
endif()

# check build prerequisites
message(CHECK_START "Checking OpenSSL build prerequisites")
list (APPEND CMAKE_MESSAGE_INDENT "   ")
unset (missing_prereq)

message(CHECK_START "Perl")
include(FindPerl) # find Perl interpreter, required for OpenSSL build scripts
if (PERL_FOUND)
	message(CHECK_PASS "found: ${PERL_EXECUTABLE}")
else()
	message(CHECK_FAIL "not found")
	list (APPEND missing_prereq "Perl")
endif()

if(MSVC)
	message(CHECK_START "nmake")
	find_program(NMAKE_EXE NAMES nmake.exe REQUIRED) # find nmake, required for building OpenSSL on Windows with MSVC
	if (EXISTS ${NMAKE_EXE})
		message(CHECK_PASS "found: ${NMAKE_EXE}")
	else()
		message(CHECK_FAIL "not found")
		list (APPEND missing_prereq "NMAKE")
	endif()

	message(CHECK_START "nasm")
	find_program(NASM_EXE NAMES nasm.exe) # find NASM assembler, required for building OpenSSL with assembly optimizations on Windows
	if (EXISTS ${NASM_EXE})
		message(CHECK_PASS "found: ${NASM_EXE}")
	else()
		message(CHECK_FAIL "not found")
		list (APPEND missing_prereq "nasm")
	endif()
elseif(UNIX)
	message(CHECK_START "make")
	find_program(MAKE_EXE NAMES make REQUIRED) # find make, required for building OpenSSL on Unix-like systems
	if (EXISTS ${MAKE_EXE})
		message(CHECK_PASS "found: ${MAKE_EXE}")
	else()
		message(CHECK_FAIL "not found")
		list (APPEND missing_prereq "make")
	endif()
endif()

list(POP_BACK CMAKE_MESSAGE_INDENT)
if (missing_prereq)
	message(CHECK_FAIL "missing components: ${missing_prereq}")
	return()
else()
	message(CHECK_PASS "all prerequisites found")
endif()

# check and fetch `OpenSSL` source code
message(CHECK_START "Fetching OpenSSL source code")
FetchContent_GetProperties(OpenSSL)
if (NOT openssl_POPULATED)
	FetchContent_Populate(OpenSSL)
	if (NOT openssl_POPULATED)
		message(CHECK_FAIL "Failed to fetch OpenSSL source code")
		return()
	endif()
endif()
message(CHECK_PASS "done")

# OpenSSL configuration:
# - For fetched OpenSSL source code, the openssl_BINARY_DIR is empty. The configuration
#   step will generate the necessary build files in openssl_BINARY_DIR.
# - If `configdata.pm` already exists, the copy of OpenSSL source code has already been
#   configured and we can skip this step.
set(original_locale ENV{LC_ALL})
set(ENV{LC_ALL} "C") # set locale to "C" to ensure consistent output from OpenSSL build scripts for parsing
set(config_result 0)
set(openssl_configdata "${openssl_BINARY_DIR}/configdata.pm")
set(openssl_need_configure TRUE)
if (MSVC)
	if (${CMAKE_GENERATOR_PLATFORM} STREQUAL "x64")
		set(platform "VC-WIN64A")
	else()
		set(platform "VC-WIN32")
	endif()
endif()

if (EXISTS ${openssl_configdata})
	set(openssl_need_configure FALSE)
	if (MSVC)
		file(READ ${openssl_configdata} configdata_text)
		if (NOT configdata_text MATCHES "\"target\" => \"${platform}\"")
			message(STATUS "Detected OpenSSL config target mismatch, reconfiguring from scratch")
			file(REMOVE_RECURSE ${openssl_BINARY_DIR})
			file(MAKE_DIRECTORY ${openssl_BINARY_DIR})
			set(openssl_need_configure TRUE)
		endif()
	endif()
endif()

if (openssl_need_configure)
	if (MSVC)
		message(STATUS "Building OpenSSL for platform ${platform}")
		execute_process(WORKING_DIRECTORY ${openssl_BINARY_DIR} COMMAND
			${PERL_EXECUTABLE} ${openssl_SOURCE_DIR}/Configure ${platform}
			--prefix=${OPENSSL_ROOT_DIR} --openssldir=SSL --api=3.0 no-docs no-deprecated no-shared
			no-pinshared no-sock no-async no-zlib no-autoload-config no-autoerrinit no-tests
			-D"_WIN32_WINNT=0x0601"
			RESULT_VARIABLE config_result
		)
	elseif(UNIX)
		execute_process(WORKING_DIRECTORY ${openssl_BINARY_DIR} COMMAND
			${openssl_SOURCE_DIR}/config
			--prefix=${OPENSSL_ROOT_DIR} --openssldir=SSL --api=3.0 no-docs no-deprecated no-shared
			no-pinshared no-sock no-async no-zlib no-autoload-config no-autoerrinit no-tests
			-fPIC -fvisibility=hidden
			RESULT_VARIABLE config_result
		)
	endif()
endif()
if (NOT config_result EQUAL 0)
	return()
endif()

# When the same source tree has been built for multiple targets over time,
# stale objects can survive and trigger LNK1112. Clean before rebuilding.
if (MSVC AND EXISTS ${openssl_BINARY_DIR}/makefile)
	execute_process(WORKING_DIRECTORY ${openssl_BINARY_DIR} COMMAND
		${NMAKE_EXE} /NOLOGO /S clean
		RESULT_VARIABLE clean_result
	)
endif()

# OpenSSL build and install:
# - Build OpenSSL using the generated build files. The build step will produce the
#   OpenSSL library files in openssl_BINARY_DIR.
# - Install OpenSSL to OPENSSL_ROOT_DIR, which will be used by find_package(OpenSSL)
#   in the main CMakeLists.txt to locate the OpenSSL library and headers.
message(CHECK_START "Prebuilding OpenSSL")
if (MSVC)
	execute_process(WORKING_DIRECTORY ${openssl_BINARY_DIR} COMMAND
		${NMAKE_EXE} /NOLOGO /S
		RESULT_VARIABLE build_result
	)
	if (${build_result} EQUAL 0)
		message(CHECK_PASS "success")
		execute_process(WORKING_DIRECTORY ${openssl_BINARY_DIR} COMMAND
			${NMAKE_EXE} /NOLOGO /S install_sw
			RESULT_VARIABLE install_result
		)
	else()
		message(CHECK_FAIL "failed")
	endif()
elseif(UNIX)
	execute_process(WORKING_DIRECTORY ${openssl_BINARY_DIR} COMMAND
		${MAKE_EXE}
		RESULT_VARIABLE build_result
	)
	if (${build_result} EQUAL 0)
		message(CHECK_PASS "success")
		execute_process(WORKING_DIRECTORY ${openssl_BINARY_DIR} COMMAND
			${MAKE_EXE} install_sw
			RESULT_VARIABLE install_result
		)
	else()
		message(CHECK_FAIL "failed")
	endif()
endif()

set(ENV{LC_ALL} ${original_locale}) # restore original locale settings
