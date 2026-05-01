# lib/port/CMakeLists.txt

set(port_SOURCES
    stpcpy.c
    strput.c
    xcalloc.c
    xstrdup.c
    socket_comm.c
    timer.cpp
    sync.cpp
    crypt.c
)

if(MSVC)
    # Windows-specific implementations
    list(APPEND port_SOURCES
        getopt.c
        gettimeofday.c
        realpath.cpp
        symlink.c
    )
endif()

add_library(port STATIC ${port_SOURCES})
target_sources(port INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ..
    FILES ansi.h debug.h wrapper.h byte_code.h timer.h socket_comm.h sync.h
)

# C++11 required for timer implementation
target_compile_features(port PUBLIC cxx_std_11)

# Thread library required for C++11 std::thread
find_package(Threads REQUIRED)
target_link_libraries(port PRIVATE Threads::Threads)

# Link OpenSSL if available for portable crypt() implementation
if(OPENSSL_FOUND)
    target_link_libraries(port PRIVATE OpenSSL::Crypto)
endif()
