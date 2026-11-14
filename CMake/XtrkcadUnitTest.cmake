# xtrkcad_add_unit_test(<target> <test> [MATH] [MAIN <file>] [FOLDER <folder>]
#                       [SOURCES <file>...] [INCLUDES <dir>...]
#                       [LIBS <library>...])
#
# One unit test: the executable <target> from MAIN (default <target>.c) and
# SOURCES, with the INCLUDES directories, linked with LIBS, xtrkcad-config
# (the generated header, which version.h and i18n.h include) and CMocka, and
# libm where there is one, with MATH; the ctest test <test>; and the IDE
# folder FOLDER (default UnitTests). A test that needs more (dependencies,
# link options, properties) adds them after the call.
function(xtrkcad_add_unit_test _target _test)
    cmake_parse_arguments(PARSE_ARGV 2 _ut "MATH" "MAIN;FOLDER"
        "SOURCES;INCLUDES;LIBS")
    if(NOT _ut_MAIN)
        set(_ut_MAIN ${_target}.c)
    endif()
    if(NOT _ut_FOLDER)
        set(_ut_FOLDER UnitTests)
    endif()
    add_executable(${_target} ${_ut_MAIN} ${_ut_SOURCES})
    if(_ut_INCLUDES)
        target_include_directories(${_target} PRIVATE ${_ut_INCLUDES})
    endif()
    target_link_libraries(${_target} PRIVATE ${_ut_LIBS} xtrkcad-config
        CMocka::cmocka)
    if(_ut_MATH)
        target_link_libraries(${_target} PRIVATE "$<${XTRKCAD_IF_UNIX}:m>")
    endif()
    add_test(${_test} ${_target})
    set_target_properties(${_target} PROPERTIES FOLDER ${_ut_FOLDER})
endfunction()
