# CMake script wrapper for cnvdsgn — called at build time.
# Variables passed via -D:
#   CNVDSGN  full path to cnvdsgn executable
#   INPUT    source .src file
#   OUTPUT   destination .lin file

if(NOT EXISTS "${INPUT}")
    message(FATAL_ERROR "run_cnvdsgn: input not found: ${INPUT}")
endif()

execute_process(
    COMMAND "${CNVDSGN}"
    INPUT_FILE  "${INPUT}"
    OUTPUT_FILE "${OUTPUT}"
    RESULT_VARIABLE _rc
    ERROR_VARIABLE  _err
)

if(_rc)
    message(STATUS "cnvdsgn stderr: ${_err}")
    message(FATAL_ERROR "cnvdsgn failed (exit ${_rc}) processing ${INPUT}")
endif()
