# lib/misc/CMakeLists.txt

set(adt_SOURCES
    avltree.c
    crc32.c
    hash.c
    qsort.c
)
add_library(misc STATIC ${adt_SOURCES})
target_include_directories(misc PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(misc PRIVATE port logger)

# each misc. header is part of the public interface
target_include_directories(misc INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
