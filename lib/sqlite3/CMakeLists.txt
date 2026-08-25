# lib/sqlite3/CMakeLists.txt
include(FetchContent)

FetchContent_Declare(
    sqlite3-amalgamation
    URL https://sqlite.org/2026/sqlite-amalgamation-3530400.zip
)
FetchContent_MakeAvailable(sqlite3-amalgamation)

add_library(sqlite3 STATIC
    ${sqlite3-amalgamation_SOURCE_DIR}/sqlite3.c
    ${sqlite3-amalgamation_SOURCE_DIR}/sqlite3.h
    ${sqlite3-amalgamation_SOURCE_DIR}/sqlite3ext.h
)
target_include_directories(sqlite3 PUBLIC
    ${sqlite3-amalgamation_SOURCE_DIR}
)
target_compile_definitions(sqlite3 PUBLIC
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_JSON1
    SQLITE_ENABLE_RTREE
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	# GCC and Clang
	target_compile_options(sqlite3 PRIVATE -Wno-implicit-fallthrough)
endif()
