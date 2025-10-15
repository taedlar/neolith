# lib/logger/CMakeLists.txt

set(logger_SOURCES
    logger.c
)

add_library(logger STATIC ${logger_SOURCES})
target_include_directories(logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
