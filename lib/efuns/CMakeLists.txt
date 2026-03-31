# lib/efuns/CMakeLists.txt

set(efuns_SOURCES
    bits.c
    call_other.c
    call_out.c
    command.c
    datetime.c
    debug.c
    dump_prog.c
    dumpstat.c
    ed.c
    file.c
    file_utils.c
    heart_beat.c
    interactive.c
    inventory.c
    maps.c
    math.c
    parse.c
    prop.c
    reclaim_object.c
    regexp.c
    replace_program.c
    sockets.c
    sprintf.c
    sscanf.c
    string.c
    tell_object.c
    uids.c
    unsorted.c
    variable.c
)

add_library(efuns STATIC ${efuns_SOURCES})
target_include_directories(efuns PUBLIC ${CMAKE_CURRENT_BINARY_DIR})    # generated efuns tables
target_include_directories(efuns INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}) # efun headers
target_include_directories(efuns PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(efuns PUBLIC port misc logger lpc rc socket)
if(HAVE_BOOST_JSON)
    target_sources(efuns PRIVATE json.cpp)
    target_link_libraries(efuns PUBLIC Boost::json)
endif()
