# lib/efuns/CMakeLists.txt

BISON_TARGET(make_func make_func.y ${CMAKE_CURRENT_BINARY_DIR}/make_func.c)

set(edit_source_SOURCES edit_source.c)
add_executable(edit_source ${edit_source_SOURCES} ${BISON_make_func_OUTPUTS})

target_include_directories(edit_source PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(edit_source PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(edit_source PRIVATE adt misc)

if(MSVC)
    set(CPP_COMMAND ${CMAKE_C_COMPILER} /I ${CMAKE_CURRENT_SOURCE_DIR} /EP)
endif()

# generate efun tables
add_custom_command(TARGET edit_source POST_BUILD
    COMMAND ${CPP_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/prototype.lpc > ${CMAKE_CURRENT_BINARY_DIR}/prototype.lpc
    COMMAND $<TARGET_FILE:edit_source> ${CMAKE_CURRENT_SOURCE_DIR}/options.h ${CMAKE_CURRENT_BINARY_DIR}/prototype.lpc
)
