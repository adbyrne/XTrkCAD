# CMake script wrapper for pngtoxpm — called at build time.
# Variables passed via -D:
#   EXE     full path to pngtoxpm executable
#   NAME    symbol name (first arg to pngtoxpm)
#   INPUT   source PNG file
#   OUTPUT  destination .image1 file

if(NOT EXISTS "${INPUT}")
    message(FATAL_ERROR "run_pngtoxpm: input PNG not found: ${INPUT}")
endif()

get_filename_component(_outdir "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_outdir}")

execute_process(
    COMMAND "${EXE}" "${NAME}" "${INPUT}" "${OUTPUT}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE  _err
)

if(_out)
    message(STATUS "${_out}")
endif()

if(_rc)
    message(STATUS "pngtoxpm stderr: ${_err}")
    message(FATAL_ERROR "pngtoxpm failed (exit ${_rc}) converting ${INPUT} -> ${OUTPUT}")
endif()
