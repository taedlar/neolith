# lib/port/CMakeLists.txt

set(port_SOURCES
    $<$<NOT:$<AND:${HAVE_STPCPY},${HAVE_STPNCPY}>>:stpcpy.c>
    xcalloc.c
    xstrdup.c
)

add_library(port STATIC ${port_SOURCES})
target_sources(port INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ..
    FILES ansi.h debug.h wrapper.h byte_code.h
)
