# lib/socket/CMakeLists.txt

set(socket_SOURCES
    socket_efuns.c
    socket_err.c
)
add_library(socket STATIC ${socket_SOURCES})
target_sources(socket INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/lib
    FILES socket_efuns.h socket_err.h
)
target_link_libraries(socket PRIVATE port misc rc)
add_dependencies(socket lpc) # for func_spec dependency

if(TARGET c-ares::cares)
	target_compile_definitions(socket PRIVATE CARES_NO_DEPRECATED)
    target_link_libraries(socket PRIVATE c-ares::cares)
endif()
