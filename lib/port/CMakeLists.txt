# lib/port/CMakeLists.txt

set(port_SOURCES
    stpcpy.c
    strput.c
    xcalloc.c
    xstrdup.c
    xvasprintf.c
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
target_sources(port PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_SOURCE_DIR}/lib
    FILES ansi.h debug.h wrapper.h byte_code.h timer.h socket_comm.h sync.h
)
target_compile_definitions(port PRIVATE NO_OPCODES)

# C++11 required for timer implementation
target_compile_features(port PUBLIC cxx_std_11)

# Thread library required for C++11 std::thread
find_package(Threads REQUIRED)
target_link_libraries(port PRIVATE Threads::Threads)

# Link OpenSSL if available for portable crypt() implementation
if(OPENSSL_FOUND)
    target_link_libraries(port PRIVATE OpenSSL::Crypto)
    
    # OpenSSL may be built with optional features that require transitive dependencies.
    # These must be PUBLIC so they propagate to any target linking against port.
    
    # Try ZLIB via find_package first (which requires development files)
    find_package(ZLIB QUIET)
    if(ZLIB_FOUND)
        target_link_libraries(port PUBLIC ZLIB::ZLIB)
    else()
        # Fall back to find_library if development package is missing
        find_library(ZLIB_LIBRARY NAMES libz.so.1 libz.so z)
        if(ZLIB_LIBRARY)
            target_link_libraries(port PUBLIC ${ZLIB_LIBRARY})
        endif()
    endif()
    
    # zstd may not have CMake config, so use find_library with explicit library names
    find_library(ZSTD_LIBRARY NAMES libzstd.so.1 libzstd.so libzstd)
    if(ZSTD_LIBRARY)
        target_link_libraries(port PUBLIC ${ZSTD_LIBRARY})
    endif()
    
    # jitterentropy is optional; only link if available
    find_library(JITTERENTROPY_LIBRARY NAMES jitterentropy)
    if(JITTERENTROPY_LIBRARY)
        target_link_libraries(port PUBLIC ${JITTERENTROPY_LIBRARY})
    endif()
endif()
