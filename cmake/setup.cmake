# cmake/setup.cmake
cmake_minimum_required(VERSION 3.28)

# This CMake script sets up the project environment by defining necessary
# variables, including paths to source directories, build directories, and
# external dependencies.
include(FetchContent)

# [ GoogleTest ]

set(gtest_force_shared_crt ON CACHE INTERNAL "Override Googletest options.")
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

# =========================
# CMake Dependency Provider
# =========================

macro(setup_provide_dependency method package)
    message(STATUS "Providing dependency: ${package} via method: ${method}")
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
endmacro()

cmake_language(
    SET_DEPENDENCY_PROVIDER setup_provide_dependency
    SUPPORTED_METHODS
        FIND_PACKAGE FETCHCONTENT_MAKEAVAILABLE_SERIAL
)
