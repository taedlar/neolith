# src/CMakeLists.txt

# The "stem" target is the collection of object files (CMake object library)
# that includes everything we need to build the LPMud driver. It can be linked
# to an executable entry function (main.c) to build the final neolith server binary.
#
# Alternatively, the "stem" object files can be linked to other frontends,
# such as a unit-test executable or a custom command line interface that functions
# as a LPC opcode compiler or other tools.

set(stem_SOURCES
    addr_resolver.cpp
    apply.c
    backend.c
    comm.c
    command.c
    error_context.cpp
    frame.c
    frame_catch.cpp
    interpret.c
    malloc.c
    outbuf.c
    simul_efun.c
    simulate.c
    stack.c
    stem.c
    stralloc.c
)
# Phase 2: error_context migrated to C++ for exception-based error handling
add_library(stem OBJECT ${stem_SOURCES})
# Phase 2: Set C++ exception handling flags for error_context.cpp
# MSVC and Clang-CL require /EHsc for synchronous exception handling
if(MSVC)
    target_compile_options(stem PRIVATE /EHsc)
endif()
target_link_libraries(stem PUBLIC port logger misc lpc efuns rc socket curl async)
if(TARGET c-ares::cares)
    target_link_libraries(stem PUBLIC c-ares::cares)
endif()
if(NOT WIN32)
    target_link_libraries(stem PUBLIC -lm)
endif()
target_include_directories(stem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ================================================================
# build neolith executable
add_executable(neolith main.c ${neolith_SOURCES})
target_link_libraries(neolith PRIVATE stem)
