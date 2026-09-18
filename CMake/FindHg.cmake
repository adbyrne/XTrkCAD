if(UNIX AND NOT APPLE)
    include(FindPackageHandleStandardArgs)
    find_program(
        Hg_EXECUTABLE
        NAMES hg
        DOC "Hg - Source control system using mercurial"
    )
    find_package_handle_standard_args(Hg REQUIRED_VARS Hg_EXECUTABLE)
    mark_as_advanced(Hg_EXECUTABLE)
endif(UNIX AND NOT APPLE)
