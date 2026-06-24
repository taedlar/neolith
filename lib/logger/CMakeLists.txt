# lib/logger/CMakeLists.txt

set(logger_SOURCES
    logger.c
)

add_library(logger STATIC ${logger_SOURCES})
target_sources(logger INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/lib
    FILES logger.h
)
target_link_libraries(logger PRIVATE port)
target_compile_definitions(logger PRIVATE NO_OPCODES)
