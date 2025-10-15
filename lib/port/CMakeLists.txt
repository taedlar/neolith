# lib/port/CMakeLists.txt

add_library(port STATIC
    stpcpy.c
    xcalloc.c
    xstrdup.c
)
target_sources(port INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ..
    FILES debug.h wrapper.h byte_code.h
)
