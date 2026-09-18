# CMake script wrapper for stripmsg — called at build time.
# Variables passed via -D:
#   EXE     full path to stripmsg executable
#   OUTPUT  destination file (replaces stdout redirection)
# Extra positional args after the script name are the input .xtq/.xtr/.tip files.

set(_files)
set(_i 0)
set(_next_is_script FALSE)
set(_past_script FALSE)
while(_i LESS CMAKE_ARGC)
    set(_arg "${CMAKE_ARGV${_i}}")
    if(NOT _past_script AND _arg STREQUAL "-P")
        set(_next_is_script TRUE)
    elseif(_next_is_script)
        set(_next_is_script FALSE)
        set(_past_script TRUE)
    elseif(_past_script)
        list(APPEND _files "${_arg}")
    endif()
    math(EXPR _i "${_i} + 1")
endwhile()

execute_process(
    COMMAND "${EXE}" ${_files}
    OUTPUT_FILE "${OUTPUT}"
    RESULT_VARIABLE _rc
    ERROR_VARIABLE  _err
)

if(_rc)
    message(STATUS "stripmsg stderr: ${_err}")
    message(FATAL_ERROR "stripmsg failed (exit ${_rc})")
endif()
