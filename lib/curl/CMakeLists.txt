# lib/curl/CMakeLists.txt

set(curl_SOURCES
    curl_efuns.cpp
)

add_library(curl OBJECT ${curl_SOURCES})
target_sources(curl PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/lib
    FILES curl_efuns.h
)
target_compile_definitions(curl PRIVATE NO_OPCODES)

if(PACKAGE_CURL)
    target_link_libraries(curl PUBLIC CURL::libcurl)
endif()
