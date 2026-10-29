# Setup GTK3 UI library...

add_subdirectory(ui)
add_subdirectory(wrapbox)
add_subdirectory(symbols)

target_include_directories(xtrkcad-wlib
    PRIVATE
    ${xtrkcad-lib_SOURCE_DIR}
)

target_sources(xtrkcad-wlib
    PRIVATE
    appwindow.c
    basicdraw.c
    bitmap.c
    builder.c
    button.c
    color.c
    combobox.c
    control.c
    dialog.c
    drawcairo.c
    drawcontrol.c
    entry.c
    expander.c
    fileselector.c
    font.c
    freetypelabel.c
    help.c
    keystate.c
    list.c
    liststore.c
    main.c
    message.c
    mrulist.c
    notebook.c
    notice.c
    opendocument.c
    popupmenu.c
    print.c
    pulldnmenu.c
    radio.c
    recentuse.c
    resourcefile.c
    revealer.c
    scalectrl.c
    separator.c
    splash.c
    stack.c
    statusbar.c
    sticky.c
    stickytogglebutton.c
    tag.c
    text.c
    timer.c
    toggle.c
    togglegroup.c
    toolbarctrl.c
    tooltip.c
    treeview.c
    util.c
    window.c
    wpref.c
)

if(WIN32)
    target_sources(xtrkcad-wlib
        PRIVATE
        win32/getline.c
        win32/getopt.c
        win32/sysinfo.c
        win32/utf8conv.c
        win32/w32paths.c
    )
endif()

if(UNIX)
    target_sources(xtrkcad-wlib
        PRIVATE
        unix/ixpaths.c
        unix/sysinfo.c
    )
endif()

# help system is OS and build specific, add appropriate source files
if(APPLE)
    if(XTRKCAD_USE_APPLEHELP)
        target_sources(xtrkcad-wlib
            PRIVATE
            osxhelp.c
        )
    elseif(XTRKCAD_USE_BROWSER)
        target_sources(xtrkcad-wlib
            PRIVATE
            browserhelp.c
        )
    else()
        message(FATAL_ERROR
            "XTRKCAD_USE_BROWSER=OFF requests the built-in WebKitGTK help viewer on "
            "APPLE, but ixhelp.c (the WebKit-1.0 help viewer implementation) was "
            "removed as dead code (unreachable: webkit-1.0 has no GTK3 equivalent, "
            "same reasoning as the non-APPLE branch below). Re-run cmake with "
            "-DXTRKCAD_USE_BROWSER=ON to show help in the system's default browser "
            "instead.")
    endif()
else()
    if(XTRKCAD_USE_BROWSER)
        target_sources(xtrkcad-wlib
            PRIVATE
            browserhelp.c
        )
    else()
        message(FATAL_ERROR
            "XTRKCAD_USE_BROWSER=OFF requests the built-in WebKitGTK help viewer, "
            "but WebKitGTK 1.0 (\"webkit-1.0\") has no GTK3 equivalent and is not "
            "available on this branch. Re-run cmake with -DXTRKCAD_USE_BROWSER=ON "
            "to show help in the system's default browser instead.")
    endif()
endif()

# GTK (PkgConfig::GTK3 and PkgConfig::FREETYPE come from the top level)
target_link_libraries(xtrkcad-wlib
    PRIVATE
    dynstring
    wrapbox
    PkgConfig::FREETYPE
    PkgConfig::GTK3
)

# GTK's native Unix print (PkgConfig::GTK_UNIX_PRINT3 comes from the top level)
if(UNIX)
    target_link_libraries(xtrkcad-wlib
        PRIVATE
        PkgConfig::GTK_UNIX_PRINT3
    )
endif()

if(TARGET resource-target AND TARGET symbols-target)
    add_dependencies(xtrkcad-wlib resource-target symbols-target)
endif()

if(XTRKCAD_TESTING)
    add_subdirectory(unittests)
endif()
