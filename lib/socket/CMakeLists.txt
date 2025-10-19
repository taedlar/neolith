# lib/socket/CMakeLists.txt

set(socket_SOURCES
    socket_ctrl.c
    socket_efuns.c
    socket_err.c
    sockets.c
)
add_library(socket STATIC ${socket_SOURCES})
target_include_directories(socket PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(socket PRIVATE port misc efuns rc)
