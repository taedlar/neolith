# lib/port/CMakeLists.txt

set(port_SOURCES
    stpcpy.c
    xcalloc.c
    xstrdup.c
    socket_comm.c
    $<$<AND:$<NOT:$<BOOL:${HAVE_LIBRT}>>,$<NOT:$<PLATFORM_ID:Windows>>>:fallback_timer.c>
)

if(MSVC)
    # Windows-specific implementations
    list(APPEND port_SOURCES
        crypt.c
        getopt.c
        gettimeofday.c
        io_reactor_win32.c
        port_sync_win32.c
        realpath.c
        symlink.c
        win32_timer.c
    )
else()
    # POSIX-compatible implementations
    list(APPEND port_SOURCES
        io_reactor_poll.c
        port_sync_pthread.c
        posix_timer.c
    )
endif()

add_library(port STATIC ${port_SOURCES})
target_sources(port INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ..
    FILES ansi.h debug.h wrapper.h byte_code.h timer_port.h socket_comm.h io_reactor.h port_sync.h
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
