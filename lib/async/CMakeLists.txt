# lib/async/CMakeLists.txt

add_library(async STATIC)

target_sources(async PRIVATE
    async_queue.c
    console_mode.c
    console_worker.c
)

if(WIN32)
    target_sources(async PRIVATE
        async_runtime_iocp.c
        async_worker_win32.c
    )
elseif(LINUX)
    target_sources(async PRIVATE
        async_runtime_epoll.c
        async_worker_pthread.c
    )
elseif(APPLE)
    target_sources(async PRIVATE
        async_runtime_poll.c
        async_worker_pthread.c
    )
endif()

target_sources(async INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/lib
    FILES async_queue.h console_mode.h console_worker.h async_worker.h async_runtime.h
)

target_link_libraries(async PUBLIC port logger)

# Platform-specific libraries
if(UNIX)
    target_link_libraries(async PRIVATE pthread)
endif()
