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

# rsvg-convert occasionally hangs on Windows, on no particular file, and a build
# runs it hundreds of times. Without a limit that stops the whole build, as
# everything else ends up waiting for this icon (Development Issues #71).
# A conversion takes well under a second, so the limit is generous for a loaded
# machine. RSVG_TIMEOUT and RSVG_ATTEMPTS are there to test this.
if(NOT DEFINED RSVG_TIMEOUT)
    set(RSVG_TIMEOUT 60)
endif()
if(NOT DEFINED RSVG_ATTEMPTS)
    set(RSVG_ATTEMPTS 3)
endif()

foreach(_attempt RANGE 1 ${RSVG_ATTEMPTS})
    execute_process(
        COMMAND "${EXE}" -w "${WIDTH}" -h "${HEIGHT}" "${INPUT}" -o "${OUTPUT}"
        TIMEOUT ${RSVG_TIMEOUT}
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE  _err
    )
    # Only a timeout is worth retrying: its result is text, not an exit code.
    if(NOT _rc MATCHES "timeout")
        break()
    endif()
    message(STATUS "rsvg-convert timed out after ${RSVG_TIMEOUT}s on ${INPUT} (attempt ${_attempt} of ${RSVG_ATTEMPTS})")
endforeach()

if(_out)
    message(STATUS "${_out}")
endif()

if(_rc)
    message(STATUS "rsvg-convert stderr: ${_err}")
    message(FATAL_ERROR "rsvg-convert failed (${_rc}) converting ${INPUT} -> ${OUTPUT}")
endif()
