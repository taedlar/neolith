# cmake/setup.cmake
cmake_minimum_required(VERSION 3.28)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)

# setup dependency provider and source of fetchable dependencies
include(FetchContent)

# [ GoogleTest ]
# set(gtest_force_shared_crt ON CACHE INTERNAL "Override Googletest options.")
# Pitfall: BUILD_SHARED_LIBS is a global cache variable shared by all subprojects.
# If a dependency flips it ON before GoogleTest is configured, tests may link to
# gtest.dll/gtest_main.dll and fail to start when those DLLs are unavailable.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Global shared/static default for dependencies" FORCE)
set(gtest_build_tests OFF CACHE INTERNAL "Override Googletest options.")
set(gtest_build_samples OFF CACHE INTERNAL "Override Googletest options.")
set(INSTALL_GTEST OFF)
set(BUILD_GMOCK OFF)
FetchContent_Declare(
    GoogleTest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG "v1.17.0"
    EXCLUDE_FROM_ALL
)

# [ OpenSSL ]
if (FETCH_OPENSSL_FROM_SOURCE)
    set(OPENSSL_ROOT_DIR "${CMAKE_BINARY_DIR}/openssl" CACHE INTERNAL "Root location for FindOpenSSL")
    FetchContent_Declare(
        OpenSSL
        GIT_REPOSITORY https://github.com/openssl/openssl.git
        GIT_TAG ${FETCH_OPENSSL_FROM_SOURCE}
        EXCLUDE_FROM_ALL
    )
endif()

# [ CURL ]
if (FETCH_CURL_FROM_SOURCE)
    # Keep fetched CURL static in this workspace and prevent it from turning on
    # global BUILD_SHARED_LIBS, which can unintentionally affect later deps
    # (for example, switching GoogleTest to DLL builds).
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Global shared/static default for dependencies" FORCE)
    set(CURL_DISABLE_TESTS ON CACHE INTERNAL "CURL option: disable build of testing code")
    set(CURL_USE_OPENSSL ON CACHE INTERNAL "CURL option: force use of openssl for consistent behaviors")
    set(CURL_USE_LIBPSL OFF CACHE INTERNAL "CURL option: disable use of libpsl")
    set(BUILD_CURL_EXE OFF CACHE INTERNAL "CURL option: disable build of curl command line program")
    set(PICKY_COMPILER OFF CACHE INTERNAL "CURL option: disable picky compiler options")
    set(HTTP_ONLY ON CACHE INTERNAL "CURL option: only enable http-related protocols")
    if(WIN32)
        set(ENABLE_UNICODE ON CACHE BOOL "Supports UNICODE.")
        set(CURL_STATIC_CRT ${USE_STATIC_MSVC_RUNTIME} CACHE BOOL "Link to static Visual C++ runtime libraries.")
        set(CURL_TARGET_WINDOWS_VERSION 0x0601 CACHE STRING "Target Windows version.")	# target Windows 7 and later by default
    endif()
    FetchContent_Declare(
        CURL
        GIT_REPOSITORY https://github.com/curl/curl.git
        GIT_TAG ${FETCH_CURL_FROM_SOURCE}
        EXCLUDE_FROM_ALL
    )
endif()

# =========================
# CMake Dependency Provider
# =========================

macro(setup_provide_dependency method package)
    message(DEBUG "Providing dependency: ${package} via method: ${method}")

    # GoogleTest
    if ("${package}" MATCHES "^(GTest|GoogleTest)$")
        # use fetched GoogleTest
        list(APPEND my_provider_args ${method} ${package}) # save arguments for macro reentrant
        FetchContent_MakeAvailable(GoogleTest)
        list(POP_BACK my_provider_args package method) # restore arguments
        if ("${method}" STREQUAL "FIND_PACKAGE")
            # import fetched package as like in module mode (FindGTest.cmake)
            set(${package}_FOUND TRUE)
            set(${package}_INCLUDE_DIRS "${googletest_SOURCE_DIR}/googletest/include")
        elseif(NOT "${package}" STREQUAL "GoogleTest")
            # adds GTest::gtest and GTest::gtest_main targets
            FetchContent_SetPopulated(${package}
                SOURCE_DIR "${googletest_SOURCE_DIR}"
                BINARY_DIR "${googletest_BINARY_DIR}"
            )
        endif()
    endif()

    # OpenSSL
    if ("${package}" MATCHES "^(OpenSSL|openssl|OPENSSL)$")
        # OpenSSL may be requested more than once (directly and transitively).
        # Skip repeated resolution after canonical imported targets exist.
        if (TARGET OpenSSL::SSL AND TARGET OpenSSL::Crypto)
            set(${package}_FOUND TRUE)
        else()
            if (FETCH_OPENSSL_FROM_SOURCE)
                # pre-build OpenSSL from source code
                list(APPEND my_provider_args ${method} ${package}) # save arguments for macro reentrant
                FetchContent_MakeAvailable(OpenSSL)
                list(POP_BACK my_provider_args package method) # restore arguments
                include(prebuild-openssl)
            endif()
            set(OPENSSL_USE_STATIC_LIBS ON)
            if (MSVC)
                set(OPENSSL_MSVC_STATIC_RT ${USE_STATIC_MSVC_RUNTIME})
            endif()
            if ("${method}" STREQUAL "FIND_PACKAGE")
                # invoke FindOpenSSL to import well-known OpenSSL variables and targets
                # keep provider-internal probe quiet; caller find_package() reports final status
                find_package(OpenSSL MODULE BYPASS_PROVIDER QUIET ${ARGN})
                if (TARGET OpenSSL::SSL AND TARGET OpenSSL::Crypto)
                    message(STATUS "Found OpenSSL: ${OPENSSL_ROOT_DIR} (found version ${OPENSSL_VERSION})")
                endif()
            endif()
            set(${package}_FOUND ${OPENSSL_FOUND})
        endif()
    endif()

    if ("${package}" MATCHES "^(CURL|curl|Curl)$")
        # CURL may be requested more than once (directly and transitively).
        # Skip repeated resolution after canonical imported target exists.
        if (TARGET CURL::libcurl)
            set(${package}_FOUND TRUE)
        else()
            if (FETCH_CURL_FROM_SOURCE)
                # pre-build CURL from source code
                list(APPEND my_provider_args ${method} ${package}) # save arguments for macro reentrant
                FetchContent_MakeAvailable(CURL)
                list(POP_BACK my_provider_args package method) # restore arguments
                if (TARGET CURL::libcurl)
                    # CURL targets are available for build with CMake. We can use them directly.
                    set(CURL_FOUND TRUE)
                    set(CURL_VERSION_STRING ${FETCH_CURL_FROM_SOURCE})
                    if ("${method}" STREQUAL "FIND_PACKAGE")
                        message(STATUS "Found CURL: ${curl_SOURCE_DIR} (found version ${CURL_VERSION_STRING})")
                    endif()
                endif()
            endif()
            set(${package}_FOUND ${CURL_FOUND})
        endif()
    endif()
endmacro()

cmake_language(
    SET_DEPENDENCY_PROVIDER setup_provide_dependency
    SUPPORTED_METHODS
        FIND_PACKAGE FETCHCONTENT_MAKEAVAILABLE_SERIAL
)
