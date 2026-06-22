# cmake/setup.cmake
cmake_minimum_required(VERSION 3.28)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)

# Ensure try_compile() checks (for example, curl's OpenSSL feature probes)
# inherit enough context to resolve OpenSSL imported targets.
set(_neolith_try_compile_vars
    CMAKE_PROJECT_TOP_LEVEL_INCLUDES
    CMAKE_FIND_PACKAGE_TARGETS_GLOBAL
    OPENSSL_ROOT_DIR
    OPENSSL_INCLUDE_DIR
    OPENSSL_SSL_LIBRARY
    OPENSSL_CRYPTO_LIBRARY
    OPENSSL_USE_STATIC_LIBS
    OPENSSL_MSVC_STATIC_RT
)
foreach(_neolith_var IN LISTS _neolith_try_compile_vars)
    if (NOT _neolith_var IN_LIST CMAKE_TRY_COMPILE_PLATFORM_VARIABLES)
        list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ${_neolith_var})
    endif()
endforeach()
unset(_neolith_try_compile_vars)
unset(_neolith_var)

function(_neolith_require_fetched_openssl_artifacts)
    if (NOT FETCH_OPENSSL_FROM_SOURCE)
        return()
    endif()

    if (NOT DEFINED OPENSSL_ROOT_DIR OR OPENSSL_ROOT_DIR STREQUAL "")
        message(FATAL_ERROR
            "FETCH_OPENSSL_FROM_SOURCE is set, but OPENSSL_ROOT_DIR is not defined."
        )
    endif()

    if (MSVC)
        set(_neolith_expected_ssl "${OPENSSL_ROOT_DIR}/lib/libssl.lib")
        set(_neolith_expected_crypto "${OPENSSL_ROOT_DIR}/lib/libcrypto.lib")
    else()
        set(_neolith_expected_ssl "${OPENSSL_ROOT_DIR}/lib/libssl.a")
        set(_neolith_expected_crypto "${OPENSSL_ROOT_DIR}/lib/libcrypto.a")
        if ((NOT EXISTS "${_neolith_expected_ssl}" OR NOT EXISTS "${_neolith_expected_crypto}")
            AND EXISTS "${OPENSSL_ROOT_DIR}/lib64/libssl.a" AND EXISTS "${OPENSSL_ROOT_DIR}/lib64/libcrypto.a")
            set(_neolith_expected_ssl "${OPENSSL_ROOT_DIR}/lib64/libssl.a")
            set(_neolith_expected_crypto "${OPENSSL_ROOT_DIR}/lib64/libcrypto.a")
        endif()
    endif()
    set(_neolith_expected_header "${OPENSSL_ROOT_DIR}/include/openssl/ssl.h")

    if (NOT EXISTS "${_neolith_expected_ssl}" OR NOT EXISTS "${_neolith_expected_crypto}" OR NOT EXISTS "${_neolith_expected_header}")
        message(FATAL_ERROR
            "FETCH_OPENSSL_FROM_SOURCE=${FETCH_OPENSSL_FROM_SOURCE} was explicitly requested, but fetched OpenSSL artifacts are unavailable. "
            "Expected files under ${OPENSSL_ROOT_DIR} (ssl: ${_neolith_expected_ssl}, crypto: ${_neolith_expected_crypto}, header: ${_neolith_expected_header}). "
            "This usually means the OpenSSL prebuild failed."
        )
    endif()

    unset(_neolith_expected_ssl)
    unset(_neolith_expected_crypto)
    unset(_neolith_expected_header)
endfunction()

function(_neolith_enforce_fetched_openssl_usage)
    if (NOT FETCH_OPENSSL_FROM_SOURCE)
        return()
    endif()

    set(_neolith_openssl_found FALSE)
    if (TARGET OpenSSL::SSL AND TARGET OpenSSL::Crypto)
        set(_neolith_openssl_found TRUE)
    endif()
    if (DEFINED OPENSSL_FOUND AND OPENSSL_FOUND)
        set(_neolith_openssl_found TRUE)
    endif()
    if (DEFINED OpenSSL_FOUND AND OpenSSL_FOUND)
        set(_neolith_openssl_found TRUE)
    endif()

    if (NOT _neolith_openssl_found)
        message(FATAL_ERROR
            "FETCH_OPENSSL_FROM_SOURCE=${FETCH_OPENSSL_FROM_SOURCE} was explicitly requested, but OpenSSL was not found."
        )
    endif()

    foreach(_neolith_openssl_path IN ITEMS "${OPENSSL_SSL_LIBRARY}" "${OPENSSL_CRYPTO_LIBRARY}" "${OPENSSL_INCLUDE_DIR}")
        if (_neolith_openssl_path STREQUAL "")
            message(FATAL_ERROR
                "FETCH_OPENSSL_FROM_SOURCE=${FETCH_OPENSSL_FROM_SOURCE} was explicitly requested, but OpenSSL resolved with empty path fields. "
                "OPENSSL_SSL_LIBRARY=${OPENSSL_SSL_LIBRARY}; OPENSSL_CRYPTO_LIBRARY=${OPENSSL_CRYPTO_LIBRARY}; OPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE_DIR}"
            )
        endif()
        string(FIND "${_neolith_openssl_path}" "${OPENSSL_ROOT_DIR}" _neolith_prefix_index)
        if (NOT _neolith_prefix_index EQUAL 0)
            message(FATAL_ERROR
                "FETCH_OPENSSL_FROM_SOURCE=${FETCH_OPENSSL_FROM_SOURCE} was explicitly requested, but OpenSSL resolved outside fetched root. "
                "OPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}; OPENSSL_SSL_LIBRARY=${OPENSSL_SSL_LIBRARY}; OPENSSL_CRYPTO_LIBRARY=${OPENSSL_CRYPTO_LIBRARY}; OPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE_DIR}"
            )
        endif()
    endforeach()

    unset(_neolith_openssl_path)
    unset(_neolith_prefix_index)
    unset(_neolith_openssl_found)
endfunction()

# setup dependency provider and source of fetchable dependencies
include(FetchContent)

# [ GoogleTest ]
#
# According to GoogleTest's documentation, users should prefer FetchContent over FindGTest.cmake
# for CMake-based projects.
set(FETCH_GOOGLETEST_FROM_SOURCE "v1.17.0" CACHE STRING "Our bundled GoogleTest version to fetch")
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
else()
    FetchContent_Declare(
        OpenSSL
        GIT_REPOSITORY https://github.com/openssl/openssl.git
        GIT_TAG openssl-4.0.1
        FIND_PACKAGE_ARGS
            NAMES OpenSSL openssl
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
    set(CURL_DISABLE_NTLM ON CACHE INTERNAL "CURL option: disable NTLM (requires deprecated DES APIs in OpenSSL)")
    set(CURL_USE_LIBPSL OFF CACHE INTERNAL "CURL option: disable use of libpsl")
    set(BUILD_CURL_EXE OFF CACHE INTERNAL "CURL option: disable build of curl command line program")
    set(BUILD_LIBCURL_DOCS OFF CACHE INTERNAL "CURL option: disable build of documentation")
    set(BUILD_MISC_DOCS OFF CACHE INTERNAL "CURL option: disable build of miscellaneous documentation")
    set(BUILD_EXAMPLES OFF CACHE INTERNAL "CURL option: disable build of examples")
    set(ENABLE_CURL_MANUAL OFF CACHE INTERNAL "CURL option: disable build of curl manual")
    set(PICKY_COMPILER OFF CACHE INTERNAL "CURL option: disable picky compiler options")
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

# [ c-ares ]
if (FETCH_CARES_FROM_SOURCE)
    # Keep fetched c-ares static in this workspace unless explicitly overridden.
    set(CARES_STATIC ON CACHE INTERNAL "c-ares option: build static library")
    set(CARES_SHARED OFF CACHE INTERNAL "c-ares option: disable shared library")
    set(CARES_BUILD_TESTS OFF CACHE INTERNAL "c-ares option: disable tests")
    set(CARES_BUILD_TOOLS OFF CACHE INTERNAL "c-ares option: disable tools")
    set(CARES_INSTALL OFF CACHE INTERNAL "c-ares option: disable install targets")
    FetchContent_Declare(
        c_ares
        GIT_REPOSITORY https://github.com/c-ares/c-ares.git
        GIT_TAG ${FETCH_CARES_FROM_SOURCE}
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
                _neolith_require_fetched_openssl_artifacts()
            endif()
            set(OPENSSL_USE_STATIC_LIBS ON)
            if (MSVC)
                set(OPENSSL_MSVC_STATIC_RT ${USE_STATIC_MSVC_RUNTIME})
            endif()
            if ("${method}" STREQUAL "FIND_PACKAGE")
                # invoke FindOpenSSL to import well-known OpenSSL variables and targets
                # keep provider-internal probe quiet; caller find_package() reports final status
                find_package(OpenSSL MODULE BYPASS_PROVIDER QUIET ${ARGN})
                _neolith_enforce_fetched_openssl_usage()
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
                # cURL's OpenSSL detection performs compile/link checks that require
                # imported OpenSSL targets to exist before cURL is configured.
                if (CURL_USE_OPENSSL AND NOT (TARGET OpenSSL::SSL AND TARGET OpenSSL::Crypto))
                    if (FETCH_OPENSSL_FROM_SOURCE)
                        list(APPEND my_provider_args ${method} OpenSSL) # save arguments for macro reentrant
                        FetchContent_MakeAvailable(OpenSSL)
                        list(POP_BACK my_provider_args package method) # restore arguments
                        include(prebuild-openssl) # ensure OPENSSL_* hints point to the prebuilt tree
                        _neolith_require_fetched_openssl_artifacts()
                    endif()
                    set(OPENSSL_USE_STATIC_LIBS ON)
                    if (MSVC)
                        set(OPENSSL_MSVC_STATIC_RT ${USE_STATIC_MSVC_RUNTIME})
                    endif()
                    find_package(OpenSSL MODULE BYPASS_PROVIDER QUIET)
                    _neolith_enforce_fetched_openssl_usage()
                endif()

                # cURL probes AWS-LC/BoringSSL/LibreSSL via check_symbol_exists()
                # while CMAKE_REQUIRED_LIBRARIES contains OpenSSL imported targets.
                # On some CMake/toolchain combinations (observed with CMake 3.31
                # in CI), the try-compile project cannot resolve those imported
                # targets and configuration fails before libcurl is generated.
                #
                # When we fetch OpenSSL from the official openssl/openssl source,
                # it is always upstream OpenSSL (not AWS-LC, BoringSSL, or
                # LibreSSL). Pre-seed these cache entries so cURL can skip those
                # symbol probes and proceed with the normal OpenSSL path.
                if (CURL_USE_OPENSSL AND FETCH_OPENSSL_FROM_SOURCE)
                    set(HAVE_AWSLC 0 CACHE INTERNAL "cURL OpenSSL probe result: fetched OpenSSL is not AWS-LC" FORCE)
                    set(HAVE_BORINGSSL 0 CACHE INTERNAL "cURL OpenSSL probe result: fetched OpenSSL is not BoringSSL" FORCE)
                    set(HAVE_LIBRESSL 0 CACHE INTERNAL "cURL OpenSSL probe result: fetched OpenSSL is not LibreSSL" FORCE)
                endif()

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

    # c-ares (compatible with find_package(c-ares CONFIG))
    if ("${package}" MATCHES "^(c-ares|CARES|cares)$")
        # c-ares may be requested more than once (directly and transitively).
        # Skip repeated resolution after canonical imported target exists.
        if (TARGET c-ares::cares)
            set(CARES_FOUND TRUE)
            set(${package}_FOUND TRUE)
        else()
            if (FETCH_CARES_FROM_SOURCE)
                list(APPEND my_provider_args ${method} ${package}) # save arguments for macro reentrant
                FetchContent_MakeAvailable(c_ares)
                list(POP_BACK my_provider_args package method) # restore arguments
                if (TARGET c-ares::cares)
                    set(CARES_FOUND TRUE)
                    set(CARES_VERSION ${FETCH_CARES_FROM_SOURCE})
                endif()
            endif()
            set(${package}_FOUND ${CARES_FOUND})
        endif()
    endif()
endmacro()

cmake_language(
    SET_DEPENDENCY_PROVIDER setup_provide_dependency
    SUPPORTED_METHODS
        FIND_PACKAGE FETCHCONTENT_MAKEAVAILABLE_SERIAL
)
