if(UNIX AND NOT APPLE)
    include(FindPackageHandleStandardArgs)
    find_program(
        Flatpak_EXECUTABLE
        NAMES flatpak
        DOC "flatpak - Build, install and run applications and runtimes"
    )
    find_program(
        Flatpak_builder_EXECUTABLE
        NAMES flatpak-builder
        DOC "flatpak-builder - Help build application dependencies"
    )
    find_package_handle_standard_args(Flatpak REQUIRED_VARS Flatpak_EXECUTABLE Flatpak_builder_EXECUTABLE)
    mark_as_advanced(Flatpak_EXECUTABLE Flatpak_builder_EXECUTABLE)
endif(UNIX AND NOT APPLE)
