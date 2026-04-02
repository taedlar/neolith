# lib/curl/CMakeLists.txt

set(curl_SOURCES
    curl_efuns.cpp
)

add_library(curl OBJECT ${curl_SOURCES})
target_include_directories(curl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(PACKAGE_CURL)
    target_link_libraries(curl PUBLIC CURL::libcurl)
endif()
