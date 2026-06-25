# cmake/utils.cmake

# This function checks for the presence of Boost libraries and sets corresponding variables
# in the parent scope.
function(find_boost required_version)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "COMPONENTS")
    if (POLICY CMP0144)
        cmake_policy(SET CMP0144 NEW)
    endif()

    if (NOT "headers" IN_LIST arg_COMPONENTS)
        list(APPEND arg_COMPONENTS "headers")
    endif()

    # CMP0144 behavior: uppercase <PACKAGENAME>_ROOT is honored by find_package.
    # BOOST_ROOT is optional and used as an override when explicitly provided.

    set(boost_override_root)
    set(boost_default_hints)
    if (BOOST_ROOT)
        set(boost_override_root "${BOOST_ROOT}")
    elseif (Boost_ROOT)
        set(boost_override_root "${Boost_ROOT}")
    endif()

    if (NOT boost_override_root)
        if (WIN32)
            # Common Boost roots and package-manager locations on Windows.
            file(GLOB boost_windows_roots
                LIST_DIRECTORIES true
                "C:/boost_*"
                "C:/local/boost_*"
                "$ENV{ProgramFiles}/boost_*"
                "$ENV{ProgramFiles}/boost/boost_*"
                "C:/Program Files (x86)/boost_*"
                "C:/Program Files (x86)/boost/boost_*"
            )
            list(APPEND boost_default_hints
                ${boost_windows_roots}
                "C:/vcpkg/installed/x64-windows/share"
                "C:/github/vcpkg/installed/x64-windows/share"
            )
        elseif(APPLE)
            list(APPEND boost_default_hints
                "/opt/homebrew/lib/cmake"
                "/usr/local/lib/cmake"
                "/opt/local/lib/cmake"
            )
        else()
            list(APPEND boost_default_hints
                "/usr/lib/x86_64-linux-gnu/cmake"
                "/usr/lib64/cmake"
                "/usr/lib/cmake"
                "/usr/local/lib/cmake"
            )
        endif()
        list(REMOVE_DUPLICATES boost_default_hints)
    endif()

    if (boost_override_root)
        set(BOOST_ROOT "${boost_override_root}")
        set(Boost_ROOT "${boost_override_root}")

        if (NOT Boost_DIR)
            file(GLOB boost_config_dirs
                LIST_DIRECTORIES true
                "${boost_override_root}/lib*/cmake/Boost-*"
            )
            list(LENGTH boost_config_dirs boost_config_dirs_count)
            if (boost_config_dirs_count EQUAL 1)
                set(Boost_DIR "${boost_config_dirs}")
            endif()
        endif()
    endif()

    if (Boost_DIR)
        find_package(Boost ${required_version} CONFIG GLOBAL
            PATHS
                ${Boost_DIR}
            NO_DEFAULT_PATH
            COMPONENTS ${arg_COMPONENTS}
        )
    elseif (boost_default_hints)
        find_package(Boost ${required_version} CONFIG GLOBAL
            HINTS
                ${boost_default_hints}
            PATH_SUFFIXES
                .
                cmake
                lib/cmake
                lib64/cmake
                lib64-msvc-14.1/cmake
                lib64-msvc-14.2/cmake
                lib64-msvc-14.3/cmake
            COMPONENTS ${arg_COMPONENTS}
        )
    else()
        find_package(Boost ${required_version} CONFIG GLOBAL COMPONENTS ${arg_COMPONENTS})
    endif()
    if (Boost_FOUND)
        set(HAVE_BOOST TRUE PARENT_SCOPE)
    endif()
    foreach(component IN LISTS arg_COMPONENTS)
        if (TARGET Boost::${component})
            string(TOUPPER ${component} component)
            set(HAVE_BOOST_${component} TRUE PARENT_SCOPE)
        endif()
    endforeach()
endfunction()

# This function checks for the presence of specific Boost headers and sets corresponding variables
# in the parent scope. It also handles the inclusion of Boost's interface include directories if
# the Boost::headers target is available.
function(find_boost_headers)
    include(CheckIncludeFileCXX)
    set(saved_required_includes ${CMAKE_REQUIRED_INCLUDES})
    set(saved_required_definitions ${CMAKE_REQUIRED_DEFINITIONS})
    set(boost_required_includes)

    # Prevent Boost headers (notably filesystem) from injecting linker requirements
    # during try_compile checks performed by check_include_file_cxx.
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DBOOST_ALL_NO_LIB)

    if (TARGET Boost::headers)
        get_target_property(boost_headers_include_dirs Boost::headers INTERFACE_INCLUDE_DIRECTORIES)
        if (boost_headers_include_dirs)
            list(APPEND boost_required_includes ${boost_headers_include_dirs})
        endif()
    endif()

    if (TARGET Boost::boost)
        get_target_property(boost_legacy_include_dirs Boost::boost INTERFACE_INCLUDE_DIRECTORIES)
        if (boost_legacy_include_dirs)
            list(APPEND boost_required_includes ${boost_legacy_include_dirs})
        endif()
    endif()

    if (Boost_INCLUDE_DIRS)
        list(APPEND boost_required_includes ${Boost_INCLUDE_DIRS})
    endif()

    if (Boost_INCLUDE_DIR)
        list(APPEND boost_required_includes ${Boost_INCLUDE_DIR})
    endif()
    if (DEFINED ENV{BOOST_INCLUDEDIR})
        list(APPEND boost_required_includes $ENV{BOOST_INCLUDEDIR})
    endif()

    if (boost_required_includes)
        list(REMOVE_DUPLICATES boost_required_includes)
        list(APPEND CMAKE_REQUIRED_INCLUDES ${boost_required_includes})
    endif()

    foreach(header IN LISTS ARGN)
        # result variable name, e.g. boost/algorithm/string.hpp -> HAVE_BOOST_ALGORITHM_STRING_HPP
        string(REPLACE "/" "_" header_var ${header})
        string(REPLACE "." "_" header_var ${header_var})
        string(TOUPPER ${header_var} header_var)
        check_include_file_cxx(${header} HAVE_${header_var})

        if (HAVE_${header_var})
            set(HAVE_${header_var} TRUE PARENT_SCOPE)
        endif()
    endforeach()

    set(CMAKE_REQUIRED_INCLUDES ${saved_required_includes})
    set(CMAKE_REQUIRED_DEFINITIONS ${saved_required_definitions})
endfunction()
