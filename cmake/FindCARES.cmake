# Finds c-ares, a C library for asynchronous DNS requests.
# author: GPT-5.3-Codex, teedlar

include(FindPackageHandleStandardArgs)

# Prefer an existing imported target first (for example, from FetchContent/provider).
if(TARGET c-ares::cares)
  set(CARES_LIBRARIES c-ares::cares)
endif()

# Try a CMake package config when available.
if(NOT TARGET c-ares::cares)
  find_package(c-ares QUIET CONFIG)
  if(TARGET c-ares::cares)
    set(CARES_LIBRARIES c-ares::cares)
  endif()
endif()

# Ubuntu/Debian libc-ares-dev commonly provides pkg-config metadata (libcares)
# but no CMake package config.
if(NOT TARGET c-ares::cares)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(PC_CARES QUIET IMPORTED_TARGET libcares)
    if(TARGET PkgConfig::PC_CARES)
      add_library(c-ares::cares INTERFACE IMPORTED)
      target_link_libraries(c-ares::cares INTERFACE PkgConfig::PC_CARES)
      set(CARES_LIBRARIES c-ares::cares)
      if(DEFINED PC_CARES_VERSION AND NOT "${PC_CARES_VERSION}" STREQUAL "")
        set(CARES_VERSION "${PC_CARES_VERSION}")
      endif()
    endif()
  endif()
endif()

# Final fallback: direct include/library lookup.
if(NOT TARGET c-ares::cares)
  find_path(CARES_INCLUDE_DIR ares.h)
  find_library(CARES_LIBRARY NAMES cares)
  if(CARES_INCLUDE_DIR AND CARES_LIBRARY)
    add_library(c-ares::cares UNKNOWN IMPORTED)
    set_target_properties(c-ares::cares PROPERTIES
      IMPORTED_LOCATION "${CARES_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${CARES_INCLUDE_DIR}")
    set(CARES_LIBRARIES c-ares::cares)
  endif()
endif()

find_package_handle_standard_args(CARES
  REQUIRED_VARS CARES_LIBRARIES
  VERSION_VAR CARES_VERSION)

mark_as_advanced(CARES_INCLUDE_DIR CARES_LIBRARY)
