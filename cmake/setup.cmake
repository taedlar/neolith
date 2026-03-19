# cmake/setup.cmake
cmake_minimum_required(VERSION 3.28)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)

# setup dependency provider and source of fetchable dependencies
include(FetchContent)

# [ GoogleTest ]
if (FETCH_GOOGLETEST_FROM_SOURCE)
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
        GIT_TAG ${FETCH_GOOGLETEST_FROM_SOURCE}
        EXCLUDE_FROM_ALL
    )
endif()

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

    # GoogleTest (compatible with FindGTest.cmake)
    if ("${package}" MATCHES "^(GTest|gtest|GoogleTest|googletest)$")
        if (TARGET GTest::gtest AND TARGET GTest::gtest_main)
            # GoogleTest targets are already available for build with CMake. We can use them directly.
            set(GTest_FOUND TRUE)
            set(${package}_FOUND ${GTest_FOUND})
        else()
            if (FETCH_GOOGLETEST_FROM_SOURCE)
                # use fetched GoogleTest
                list(APPEND my_provider_args ${method} ${package}) # save arguments for macro reentrant
                FetchContent_MakeAvailable(GoogleTest)
                list(POP_BACK my_provider_args package method) # restore arguments
                if (TARGET GTest::gtest AND TARGET GTest::gtest_main)
                    # GoogleTest targets are available for build with CMake. We can use them directly.
                    set(GTest_FOUND TRUE)
                    set(GTest_INCLUDE_DIRS "${googletest_SOURCE_DIR}/googletest/include")
                    set(GTest_VERSION ${FETCH_GOOGLETEST_FROM_SOURCE})
                endif()
            endif()
            set(${package}_FOUND ${GTest_FOUND})
        endif()
    endif()

    # OpenSSL (compatible with FindOpenSSL.cmake)
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
                include(prebuild-openssl) # prebuild and install to OPENSSL_ROOT_DIR for FindOpenSSL to find
            endif()
            set(OPENSSL_USE_STATIC_LIBS ON)
            if (MSVC)
                set(OPENSSL_MSVC_STATIC_RT ${USE_STATIC_MSVC_RUNTIME})
            endif()
            if ("${method}" STREQUAL "FIND_PACKAGE")
                # invoke FindOpenSSL to import well-known OpenSSL variables and targets
                # keep provider-internal probe quiet; caller find_package() reports final status
                find_package(OpenSSL MODULE BYPASS_PROVIDER QUIET ${ARGN})
            endif()
            set(${package}_FOUND ${OPENSSL_FOUND})
        endif()
    endif()

    # cURL (compatible with FindCURL.cmake)
    if ("${package}" MATCHES "^(CURL|curl|cURL|Curl)$")
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
                    set(CURL_INCLUDE_DIRS "${curl_SOURCE_DIR}/include")
                    set(CURL_VERSION_STRING ${FETCH_CURL_FROM_SOURCE})
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
