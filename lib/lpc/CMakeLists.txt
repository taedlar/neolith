# lib/lpc/CMakeLists.txt

BISON_TARGET(make_func make_func.y ${CMAKE_CURRENT_BINARY_DIR}/make_func.c)

set(edit_source_SOURCES edit_source.c)
add_executable(edit_source ${edit_source_SOURCES} ${BISON_make_func_OUTPUTS})

target_include_directories(edit_source PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(edit_source PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(edit_source PRIVATE port misc)
configure_file(func_spec.c.in ${CMAKE_CURRENT_BINARY_DIR}/func_spec.c @ONLY)
configure_file(options.h.in ${CMAKE_BINARY_DIR}/options.h @ONLY)

# preprocessor-only command
if(MSVC)
    set(CPP_COMMAND ${CMAKE_C_COMPILER} /DHAVE_CONFIG_H
        /I ${CMAKE_CURRENT_SOURCE_DIR} /I ${CMAKE_BINARY_DIR} /I ${CMAKE_CURRENT_BINARY_DIR} /EP) # cl
else()
    set(CPP_COMMAND ${CMAKE_C_COMPILER} -DHAVE_CONFIG_H -x c
        -I ${CMAKE_CURRENT_SOURCE_DIR} -I ${CMAKE_BINARY_DIR} -I ${CMAKE_CURRENT_BINARY_DIR} -E)  # gcc
endif()

# generate preprocessed func_spec.i (configured from func_spec.c.in and includes options.h)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/func_spec.i
    COMMAND ${CPP_COMMAND} ${CMAKE_CURRENT_BINARY_DIR}/func_spec.c > ${CMAKE_CURRENT_BINARY_DIR}/func_spec.i
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/func_spec.c ${CMAKE_BINARY_DIR}/options.h
)
add_custom_target(generate_func_spec_i DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/func_spec.i)
add_dependencies(edit_source generate_func_spec_i)

# generate efun tables at ${CMAKE_BINARY_DIR}
add_custom_command(TARGET edit_source POST_BUILD
    COMMAND $<TARGET_FILE:edit_source> ${CMAKE_BINARY_DIR}/options.h ${CMAKE_CURRENT_BINARY_DIR}/func_spec.i
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

BISON_TARGET(grammar grammar.y ${CMAKE_CURRENT_BINARY_DIR}/grammar.c
    DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/grammar.h
)
add_custom_target(generate_grammar DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/grammar.h) # for generated header dependency

set(lpc_SOURCES
    array.c
    buffer.c
    class.c
    compiler.c
    functional.cpp
    identifier.c
    lex.c
    mapping.c
    object.c
    operator.c
    otable.c
    svalue.c
    program.c
    program/binaries.c
    program/disassemble.c
    program/generate.c
    program/icode.c
    program/parse_trees.c
)
add_library(lpc STATIC ${lpc_SOURCES} ${BISON_grammar_OUTPUTS})
add_dependencies(lpc edit_source)
target_sources(lpc INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ..
    FILES array.h buffer.h class.h functional.h mapping.h object.h otable.h program.h svalue.h types.h
)
target_link_libraries(lpc PUBLIC port misc logger efuns rc)
target_include_directories(lpc PUBLIC ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
