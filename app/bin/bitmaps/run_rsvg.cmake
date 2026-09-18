# CMake script wrapper for rsvg-convert — called at build time.
# Variables passed via -D:
#   EXE     full path to rsvg-convert executable
#   WIDTH   output pixel width
#   HEIGHT  output pixel height
#   INPUT   source SVG file
#   OUTPUT  destination PNG file

if(NOT EXISTS "${INPUT}")
    message(FATAL_ERROR "run_rsvg: input SVG not found: ${INPUT}")
endif()

get_filename_component(_outdir "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_outdir}")

execute_process(
    COMMAND "${EXE}" -w "${WIDTH}" -h "${HEIGHT}" "${INPUT}" -o "${OUTPUT}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE  _err
)

if(_out)
    message(STATUS "${_out}")
endif()

if(_rc)
    message(STATUS "rsvg-convert stderr: ${_err}")
    message(FATAL_ERROR "rsvg-convert failed (exit ${_rc}) converting ${INPUT} -> ${OUTPUT}")
endif()
