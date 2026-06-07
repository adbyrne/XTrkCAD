# CMake script wrapper for gdk-pixbuf-csource — called at build time.
# Variables passed via -D:
#   SYMBOL   C identifier passed as --name argument
#   INPUT    source PNG file
#   OUTPUT   destination .image1 file

if(NOT EXISTS "${INPUT}")
    message(FATAL_ERROR "run_gdk_pixbuf: input PNG not found: ${INPUT}")
endif()

execute_process(
    COMMAND gdk-pixbuf-csource --stream --name "${SYMBOL}" "${INPUT}"
    OUTPUT_FILE "${OUTPUT}"
    RESULT_VARIABLE _rc
    ERROR_VARIABLE  _err
)

if(_rc)
    message(STATUS "gdk-pixbuf-csource stderr: ${_err}")
    message(FATAL_ERROR "gdk-pixbuf-csource failed (exit ${_rc}) on ${INPUT}")
endif()
