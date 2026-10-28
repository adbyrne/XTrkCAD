set(CPACK_SOURCE_GENERATOR "ZIP")

# GTK3/MinGW builds require runtime data files beyond DLLs.
# DLLs themselves are handled by RUNTIME_DEPENDENCY_SET in app/bin/CMakeLists.txt.
if(MINGW)
    get_filename_component(MINGW_BIN "${CMAKE_C_COMPILER}" DIRECTORY)
    get_filename_component(MINGW_PREFIX "${MINGW_BIN}" DIRECTORY)

    # GDK pixbuf loaders — required for SVG icons and PNG toolbar images
    if(EXISTS "${MINGW_PREFIX}/lib/gdk-pixbuf-2.0")
        install(DIRECTORY "${MINGW_PREFIX}/lib/gdk-pixbuf-2.0"
            DESTINATION lib)
    endif()

    # GSettings compiled schemas — GTK3 on Windows needs this to start
    if(EXISTS "${MINGW_PREFIX}/share/glib-2.0/schemas/gschemas.compiled")
        install(FILES "${MINGW_PREFIX}/share/glib-2.0/schemas/gschemas.compiled"
            DESTINATION share/glib-2.0/schemas)
    endif()

    # Hicolor icon theme — GTK3 fallback icons
    if(EXISTS "${MINGW_PREFIX}/share/icons/hicolor")
        install(DIRECTORY "${MINGW_PREFIX}/share/icons/hicolor"
            DESTINATION share/icons)
    endif()
endif()

# MSVC builds against gvsbuild: the same data comes from the GTK prefix that
# pkg-config reported. GTK also needs its default theme's icons from there.
if(MSVC)
    if(EXISTS "${GTK3_PREFIX}/lib/gdk-pixbuf-2.0")
        install(DIRECTORY "${GTK3_PREFIX}/lib/gdk-pixbuf-2.0"
            DESTINATION lib)

        # GTK loads the pixbuf loaders at run time, so the executable's
        # dependency scan (app/bin) misses their DLLs, e.g. librsvg and
        # libxml2 for the SVG loader. Scan the loaders themselves.
        file(GLOB_RECURSE _pixbuf_loaders
            "${GTK3_PREFIX}/lib/gdk-pixbuf-2.0/*.dll")
        if(_pixbuf_loaders)
            install(CODE "set(_xtc_loaders \"${_pixbuf_loaders}\")")
            install(CODE "set(_xtc_gtkbin \"${GTK3_PREFIX}/bin\")")
            install(CODE [[
                file(GET_RUNTIME_DEPENDENCIES
                    MODULES ${_xtc_loaders}
                    DIRECTORIES "${_xtc_gtkbin}"
                    PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
                    POST_EXCLUDE_REGEXES ".*[Ss]ystem32/.*\\.dll"
                    RESOLVED_DEPENDENCIES_VAR _xtc_deps
                    UNRESOLVED_DEPENDENCIES_VAR _xtc_missing)
                if(_xtc_missing)
                    message(WARNING "DLLs for the gdk-pixbuf loaders not found in ${_xtc_gtkbin}: ${_xtc_missing}")
                endif()
                file(INSTALL ${_xtc_deps} DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
            ]])
        endif()
    endif()

    if(EXISTS "${GTK3_PREFIX}/share/glib-2.0/schemas/gschemas.compiled")
        install(FILES "${GTK3_PREFIX}/share/glib-2.0/schemas/gschemas.compiled"
            DESTINATION share/glib-2.0/schemas)
    endif()

    foreach(_theme hicolor Adwaita)
        if(EXISTS "${GTK3_PREFIX}/share/icons/${_theme}")
            install(DIRECTORY "${GTK3_PREFIX}/share/icons/${_theme}"
                DESTINATION share/icons)
        endif()
    endforeach()

    if(EXISTS "${GTK3_PREFIX}/etc/gtk-3.0")
        install(DIRECTORY "${GTK3_PREFIX}/etc/gtk-3.0"
            DESTINATION etc)
    endif()
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/README-INSTALL.txt.in" "${CMAKE_CURRENT_BINARY_DIR}/README-INSTALL.txt" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/README-INSTALL.txt"
    DESTINATION ".")

add_subdirectory(nsis)
