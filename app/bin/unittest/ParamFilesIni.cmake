# Helper for the RegressionParams.* tests (RegressionTests.cmake): every
# parameter file installed with the program has to load. Run with cmake -P.
#
#   -DMODE=write -DPARAMS_DIR=<dir> -DINI=<file>
#       Writes a preferences file that lists every .xtp in PARAMS_DIR, so the
#       program loads them all at startup (LoadParamFileList()). Fails on a
#       file with no CONTENTS line or with a title another file already has:
#       the list is keyed by title, so such a file can't be selected.
#   -DMODE=check -DPARAMS_DIR=<dir> -DINI=<file>
#       After the run, the program has saved the list of files it loaded.
#       Fails unless every file is still in it: a file that doesn't load is
#       dropped from the list.

if(NOT MODE OR NOT PARAMS_DIR OR NOT INI)
    message(FATAL_ERROR "ParamFilesIni.cmake: MODE, PARAMS_DIR and INI are required")
endif()

file(GLOB _files "${PARAMS_DIR}/*.xtp")
list(SORT _files)
list(LENGTH _files _count)
if(_count EQUAL 0)
    message(FATAL_ERROR "No .xtp files in ${PARAMS_DIR}")
endif()

if(MODE STREQUAL "write")
    # The files are Latin-1, which file(STRINGS) cuts at the first non-ASCII
    # byte, so read the raw text. Titles can hold ';', so no CMake lists.
    set(_map "\n")
    set(_names "")
    set(_errors "")
    set(_n 0)
    foreach(_file ${_files})
        get_filename_component(_base "${_file}" NAME)
        file(READ "${_file}" _text)
        string(REGEX MATCH "(^|\n)CONTENTS +[^\r\n]*" _contents "${_text}")
        if(NOT _contents)
            string(APPEND _errors "  ${_base}: no CONTENTS line\n")
            continue()
        endif()
        string(REGEX REPLACE "^\n?CONTENTS +" "" _title "${_contents}")
        string(STRIP "${_title}" _title)
        string(FIND "${_map}" "\n${_title}=" _seen)
        if(NOT _seen EQUAL -1)
            string(APPEND _errors "  ${_base}: title \"${_title}\" is used by another file\n")
            continue()
        endif()
        math(EXPR _n "${_n} + 1")
        string(APPEND _map "${_title}=${_file}\n")
        string(APPEND _names "File${_n}=${_title}\n")
    endforeach()
    if(_errors)
        message(FATAL_ERROR "Parameter files that can't be listed:\n${_errors}")
    endif()
    get_filename_component(_dir "${INI}" DIRECTORY)
    file(MAKE_DIRECTORY "${_dir}")
    file(WRITE "${INI}"
        "[misc]\nfirstrun=0\n"
        "[Parameter File Map]${_map}"
        "[Parameter File Names]\n${_names}")
    message(STATUS "Listed ${_n} parameter files in ${INI}")

elseif(MODE STREQUAL "check")
    if(NOT EXISTS "${INI}")
        message(FATAL_ERROR "${INI} not found: did the program run?")
    endif()
    # The saved map has one line per loaded file, title=path. Titles are
    # Latin-1 and can hold ';', and the program changes some characters in
    # them when it saves (SaveParamFileList()), so look for each path in the
    # raw text of that section instead.
    file(READ "${INI}" _ini)
    string(REPLACE "\r" "" _ini "${_ini}")
    string(FIND "${_ini}" "[Parameter File Map]\n" _begin)
    if(_begin EQUAL -1)
        message(FATAL_ERROR "${INI} has no [Parameter File Map] section")
    endif()
    string(SUBSTRING "${_ini}" ${_begin} -1 _section)
    string(FIND "${_section}" "\n[" _end)
    if(NOT _end EQUAL -1)
        math(EXPR _end "${_end} + 1")
        string(SUBSTRING "${_section}" 0 ${_end} _section)
    endif()
    string(REPLACE "\\" "/" _section "${_section}")
    set(_missing "")
    foreach(_file ${_files})
        file(TO_CMAKE_PATH "${_file}" _path)
        string(FIND "${_section}" "=${_path}\n" _found)
        if(_found EQUAL -1)
            get_filename_component(_base "${_file}" NAME)
            string(APPEND _missing "  ${_base}\n")
        endif()
    endforeach()
    if(_missing)
        message(FATAL_ERROR "Parameter files that did not load:\n${_missing}")
    endif()
    message(STATUS "All ${_count} parameter files loaded")

else()
    message(FATAL_ERROR "ParamFilesIni.cmake: unknown MODE ${MODE}")
endif()
