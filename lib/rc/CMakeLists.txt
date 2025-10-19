# lib/rc/CMakeLists.txt

set(rc_SOURCES
    rc.cpp
)
add_library(rc STATIC ${rc_SOURCES})
target_include_directories(rc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rc PRIVATE port logger)
