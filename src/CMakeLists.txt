# src/CMakeLists.txt

BISON_TARGET(grammar grammar.y ${CMAKE_CURRENT_BINARY_DIR}/grammar.c
    COMPILE_FLAGS --defines=${CMAKE_CURRENT_BINARY_DIR}/grammar.h
)

set(neolith_SOURCES
    backend.c
    binaries.c
    comm.c
    compiler.c
    generate.c
    icode.c
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
    trees.c
    uids.c
)

add_executable(neolith ${neolith_SOURCES} ${BISON_grammar_OUTPUTS})
target_link_libraries(neolith PRIVATE port logger misc efuns lpc socket -lm -lcrypt)
target_include_directories(neolith PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}
)
