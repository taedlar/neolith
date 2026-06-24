# lib/efuns/CMakeLists.txt

set(efuns_SOURCES
    bits.c
    call_other.c
    call_out.c
    command.cpp
    datetime.c
    debug.c
    dump_prog.c
    dumpstat.c
    file.c
    file_utils.c
    heart_beat.c
    interactive.c
    inventory.c
    maps.c
    math.c
    objects.cpp
    parse.c
    prop.c
    reclaim_object.c
    replace_program.c
    sprintf.c
    sscanf.c
    string.c
    tell_object.c
    uids.c
    unsorted.c
    variable.c
)

add_library(efuns STATIC ${efuns_SOURCES})
target_sources(efuns INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/lib
    FILES dumpstat.h file_utils.h parse.h reclaim_object.h replace_program.h sscanf.h uids.h
)

if(PACKAGE_SOCKETS)
    target_sources(efuns PRIVATE sockets.c)
    target_link_libraries(efuns PRIVATE socket)
endif()

target_link_libraries(efuns PUBLIC port misc logger lpc rc curl)

if(HAVE_BOOST_JSON)
    target_sources(efuns PRIVATE json.cpp)
    if (BOOST_JSON_HEADER_ONLY OR TARGET Boost::json)
        # Suppress Boost's MSVC autolink pragma so the linker uses only the
        # full-path library that CMake resolves via the Boost::json target.
        # BOOST_ALL_NO_LIB is a no-op on non-MSVC compilers (Linux/Clang),
        # where the pragma-based autolink mechanism does not exist.
        target_compile_definitions(efuns PRIVATE BOOST_ALL_NO_LIB)
    endif()
    if(TARGET Boost::json)
        target_link_libraries(efuns PUBLIC Boost::json)
    elseif(TARGET Boost::headers)
        target_link_libraries(efuns PRIVATE Boost::headers)
    elseif(TARGET Boost::boost)
        target_link_libraries(efuns PRIVATE Boost::boost)
    endif()
elseif(HAVE_JSONCPP)
    target_sources(efuns PRIVATE json.cpp)
    target_link_libraries(efuns PRIVATE json)
endif()
