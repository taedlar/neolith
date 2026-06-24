# lib/misc/CMakeLists.txt

set(adt_SOURCES
    avltree.c
    crc32.c
    envsubst.c
    filepath.cpp
    hash.c
    qsort.c
    scratchpad.c
)
add_library(misc STATIC ${adt_SOURCES})

# each misc. header is part of the public interface
target_sources(misc PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/lib
    FILES avltree.h crc32.h envsubst.h filepath.h hash.h qsort.h scratchpad.h
)
