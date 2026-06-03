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

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/README-INSTALL.txt"
    DESTINATION ".")

add_subdirectory(nsis)
