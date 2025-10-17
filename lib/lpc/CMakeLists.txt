# lib/lpc/CMakeLists.txt

set(lpc_SOURCES
    array.c
    buffer.c
    class.c
    mapping.c
    object.c
    operator.c
    otable.c
    program.c
    program/binaries.c
    program/disassemble.c
    program/icode.c
    program/parse_trees.c
)
add_library(lpc STATIC ${lpc_SOURCES})
target_sources(lpc INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ..
    FILES array.h buffer.h class.h functional.h mapping.h object.h otable.h program.h types.h
)
target_include_directories(lpc PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(lpc PUBLIC
    logger
    efuns
)
