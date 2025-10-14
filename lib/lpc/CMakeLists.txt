# lib/lpc/CMakeLists.txt

set(lpc_SOURCES
    array.c
    buffer.c
    class.c
    disassemble.c
    mapping.c
    object.c
    otable.c
)
add_library(lpc STATIC ${lpc_SOURCES})
target_include_directories(lpc PUBLIC ${CMAKE_SOURCE_DIR}/lib)
target_include_directories(lpc PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib
)
target_link_libraries(lpc PUBLIC
    efuns
)
