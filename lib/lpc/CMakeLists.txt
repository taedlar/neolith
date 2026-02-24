# lib/lpc/CMakeLists.txt

BISON_TARGET(grammar grammar.y ${CMAKE_CURRENT_BINARY_DIR}/grammar.c
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/grammar.h
)

set(lpc_SOURCES
    array.c
    buffer.c
    class.c
    compiler.c
    functional.c
    identifier.c
    lex.c
    mapping.c
    object.c
    operator.c
    otable.c
    svalue.c
    program.c
    program/binaries.c
    program/disassemble.c
    program/generate.c
    program/icode.c
    program/parse_trees.c
)
add_library(lpc STATIC ${lpc_SOURCES} ${BISON_grammar_OUTPUTS})
target_sources(lpc INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ..
    FILES array.h buffer.h class.h functional.h mapping.h object.h otable.h program.h svalue.h types.h
)
target_include_directories(lpc PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${PROJECT_BINARY_DIR}
)
target_link_libraries(lpc PUBLIC
    logger
    efuns
    rc
)
