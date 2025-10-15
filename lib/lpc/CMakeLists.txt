# lib/lpc/CMakeLists.txt

set(lpc_SOURCES
    array.c
    buffer.c
    class.c
    disassemble.c
    mapping.c
    object.c
    otable.c
    program.c
)
add_library(lpc STATIC ${lpc_SOURCES})
target_sources(lpc INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ..
    FILES array.h buffer.h class.h functional.h mapping.h object.h program.h types.h
)
target_include_directories(lpc PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(lpc PUBLIC
    logger
    efuns
)
