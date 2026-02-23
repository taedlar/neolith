# src/CMakeLists.txt

# The "stem" target is the collection of object files (CMake object library)
# that includes everything we need to build the LPMud driver. It can be linked
# to an executable entry function (main.c) to build the final neolith server binary.
#
# Alternatively, the "stem" object files can be linked to other frontends,
# such as a unit-test executable or a custom command line interface that functions
# as a LPC opcode compiler or other tools.

set(stem_SOURCES
    apply.c
    backend.c
    comm.c
    command.c
    error_context.c
    frame.c
    interpret.c
    malloc.c
    outbuf.c
    simul_efun.c
    simulate.c
    stack.c
    stem.c
    stralloc.c
)
add_library(stem OBJECT ${stem_SOURCES})
target_link_libraries(stem PUBLIC port logger misc lpc efuns rc socket async)
if(NOT WIN32)
    target_link_libraries(stem PUBLIC -lm -lcrypt)
endif()
target_include_directories(stem PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

# ================================================================
# build neolith executable
add_executable(neolith main.c ${neolith_SOURCES})
target_link_libraries(neolith PRIVATE stem)
