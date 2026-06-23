# lib/json/CMakeLists.txt
#
# JsonCpp amalagamation source.  We build it as an object library to avoid the need
# for a separate shared library and to allow it to be linked into multiple targets
# without duplication.
#
# Original source: https://github.com/open-source-parsers/jsoncpp.git

set(json_SOURCES jsoncpp.cpp)
add_library(json OBJECT ${json_SOURCES})
target_sources(json PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/lib
    FILES json.h
)
target_include_directories(json PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(json PRIVATE NO_OPCODES)
