# src/CMakeLists.txt

BISON_TARGET(grammar grammar.y ${CMAKE_CURRENT_BINARY_DIR}/grammar.c
    COMPILE_FLAGS --defines=${CMAKE_CURRENT_BINARY_DIR}/grammar.h
)

set(neolith_SOURCES
    backend.c
    comm.c
    compiler.c
    generate.c
    interpret.c
    lex.c
    main.c
    malloc.c
    rc.c
    regexp.c
    scratchpad.c
    simul_efun.c
    simulate.c
    stralloc.c
    uids.c
)

add_executable(neolith ${neolith_SOURCES} ${BISON_grammar_OUTPUTS})
target_link_libraries(neolith PRIVATE port logger misc efuns lpc socket -lm -lcrypt)
target_include_directories(neolith PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}
)

# check POSIX realtime extension library
include(CheckLibraryExists)
check_library_exists(rt clock_gettime "" HAVE_LIBRT)
if(HAVE_LIBRT)
    target_link_libraries(neolith PRIVATE -lrt)
else()
    message(WARNING "LIBRT not found, heart_beat() will be disabled.")
endif()
