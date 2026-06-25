# lib/rc/CMakeLists.txt

set(rc_SOURCES
    rc.cpp
)
add_library(rc STATIC ${rc_SOURCES})
target_sources(rc PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/lib
    FILES rc.h
)
target_link_libraries(rc PRIVATE port logger)
target_compile_definitions(rc PRIVATE NO_OPCODES)
