# Async Library
# Platform-agnostic async runtime for offloading blocking operations

add_library(async STATIC)

target_sources(async PRIVATE
    async_queue.c
    console_worker.c
    $<IF:$<PLATFORM_ID:Windows>,async_worker_win32.c,async_worker_pthread.c>
    $<IF:$<PLATFORM_ID:Windows>,async_runtime_iocp.c,$<IF:$<PLATFORM_ID:Linux>,async_runtime_epoll.c,async_runtime_poll.c>>
)

target_include_directories(async PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(async PUBLIC
    port
    logger
)

# Platform-specific libraries
if(UNIX)
    target_link_libraries(async PRIVATE pthread)
endif()
