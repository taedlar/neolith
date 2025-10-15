# lib/logger/CMakeLists.txt

set(logger_SOURCES
    logger.c
)

add_library(logger STATIC ${logger_SOURCES})
target_sources(logger INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ..
    FILES logger.h
)
