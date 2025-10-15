# lib/adt/CMakeLists.txt

set(adt_SOURCES
    avltree.c
    crc32.c
    hash.c
    qsort.c
)
add_library(adt STATIC ${adt_SOURCES})
target_include_directories(adt PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(adt PRIVATE port logger)

target_include_directories(adt INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
