# lib/efuns/CMakeLists.txt

BISON_TARGET(make_func make_func.y ${CMAKE_CURRENT_BINARY_DIR}/make_func.c)

set(edit_source_SOURCES edit_source.c)
add_executable(edit_source ${edit_source_SOURCES} ${BISON_make_func_OUTPUTS})

target_include_directories(edit_source PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(edit_source PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(edit_source PRIVATE port misc)

# preprocessor-only command
if(MSVC)
    set(CPP_COMMAND ${CMAKE_C_COMPILER} /I ${CMAKE_CURRENT_SOURCE_DIR} /EP) # cl
else()
    set(CPP_COMMAND ${CMAKE_C_COMPILER} -I ${CMAKE_CURRENT_SOURCE_DIR} -E)  # gcc
endif()

# generate efun tables
add_custom_command(TARGET edit_source POST_BUILD
    COMMAND ${CPP_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/func_spec.c > ${CMAKE_CURRENT_BINARY_DIR}/func_spec.i
    COMMAND $<TARGET_FILE:edit_source> ${CMAKE_CURRENT_SOURCE_DIR}/options.h ${CMAKE_CURRENT_BINARY_DIR}/func_spec.i
)

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
add_dependencies(efuns edit_source)
target_include_directories(efuns PUBLIC ${CMAKE_CURRENT_BINARY_DIR})    # generated efuns tables
target_include_directories(efuns INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}) # efun headers
target_include_directories(efuns PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(efuns PUBLIC port misc logger lpc rc socket)
