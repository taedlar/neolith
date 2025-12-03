# lib/port/CMakeLists.txt

set(port_SOURCES
    $<$<NOT:$<AND:$<BOOL:${HAVE_STPCPY}>,$<BOOL:${HAVE_STPNCPY}>>>:stpcpy.c>
    xcalloc.c
    xstrdup.c
    $<$<PLATFORM_ID:Windows>:crypt.c>
    $<$<PLATFORM_ID:Windows>:getopt.c>
    $<$<PLATFORM_ID:Windows>:gettimeofday.c>
    $<$<PLATFORM_ID:Windows>:realpath.c>
    $<$<PLATFORM_ID:Windows>:symlink.c>
    $<$<PLATFORM_ID:Windows>:win32_timer.c>
    $<$<AND:$<BOOL:${HAVE_LIBRT}>,$<NOT:$<PLATFORM_ID:Windows>>>:posix_timer.c>
    $<$<AND:$<NOT:$<BOOL:${HAVE_LIBRT}>>,$<NOT:$<PLATFORM_ID:Windows>>>:fallback_timer.c>
)

add_library(port STATIC ${port_SOURCES})
target_sources(port INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ..
    FILES ansi.h debug.h wrapper.h byte_code.h timer_port.h
)

# Windows-specific libraries for timer support
if(WIN32)
    target_link_libraries(port PRIVATE kernel32)
endif()

# POSIX systems may need librt for timer functions
if(HAVE_LIBRT)
    target_link_libraries(port PRIVATE rt)
endif()

# Fallback timer needs pthread on non-Windows systems
if(NOT WIN32 AND NOT HAVE_LIBRT)
    find_package(Threads REQUIRED)
    target_link_libraries(port PRIVATE Threads::Threads)
endif()
