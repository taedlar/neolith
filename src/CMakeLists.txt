# src/CMakeLists.txt

set(neolith_SOURCES
    backend.c
    comm.c
    interpret.c
    main.c
    malloc.c
    regexp.c
    simul_efun.c
    simulate.c
    stralloc.c
    uids.c
)

add_executable(neolith ${neolith_SOURCES})
target_link_libraries(neolith PRIVATE port logger misc efuns lpc rc socket -lm -lcrypt)
target_include_directories(neolith PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

# check POSIX realtime extension library
include(CheckLibraryExists)
check_library_exists(rt clock_gettime "" HAVE_LIBRT)
if(HAVE_LIBRT)
    target_link_libraries(neolith PRIVATE -lrt)
else()
    message(WARNING "LIBRT not found, heart_beat() will be disabled.")
endif()
