# src/CMakeLists.txt

# The "stem" target is the collection of object files (CMake object library)
# that includes everything we need to build the LPMud driver. It can be linked
# to an executable entry function (main.c) to build the final neolith server binary.
#
# Alternatively, the "stem" object files can be linked to other frontends,
# such as a unit-test executable or a custom command line interface that functions
# as a LPC opcode compiler or other tools.

set(stem_SOURCES
    stem.c
    backend.c
    comm.c
    interpret.c
    malloc.c
    regexp.c
    simul_efun.c
    simulate.c
    stralloc.c
)
add_library(stem OBJECT ${stem_SOURCES})
target_link_libraries(stem PUBLIC port logger misc lpc efuns rc socket -lm -lcrypt)
target_include_directories(stem PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

# check POSIX realtime extension library
include(CheckLibraryExists)
check_library_exists(rt clock_gettime "" HAVE_LIBRT)
if(HAVE_LIBRT)
    target_link_libraries(stem PUBLIC -lrt)
else()
    message(WARNING "LIBRT not found, heart_beat() will be disabled.")
endif()

# ================================================================
# build neolith executable
add_executable(neolith main.c ${neolith_SOURCES})
target_link_libraries(neolith PRIVATE stem)
