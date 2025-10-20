# src/CMakeLists.txt

set(stem_SOURCES
    stem.c
    backend.c
    comm.c
    interpret.c
    malloc.c
    regexp.c
    simul_efun.c
    simulate.c
    stralloc.c
    uids.c
)
add_library(stem OBJECT ${stem_SOURCES})
target_link_libraries(stem PUBLIC port logger misc lpc efuns rc socket -lm -lcrypt)
target_include_directories(stem PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

# check POSIX realtime extension library
include(CheckLibraryExists)
check_library_exists(rt clock_gettime "" HAVE_LIBRT)
if(HAVE_LIBRT)
    target_link_libraries(stem PUBLIC -lrt)
else()
    message(WARNING "LIBRT not found, heart_beat() will be disabled.")
endif()

# build neolith executable
add_executable(neolith main.c ${neolith_SOURCES})
target_link_libraries(neolith PRIVATE stem)
