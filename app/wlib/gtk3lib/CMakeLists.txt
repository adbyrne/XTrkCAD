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
 button.c
 color.c
 combobox.c
 drawcontrol.c
 entry.c
 expander.c
  fileselector.c
 freetypelabel.c
 keystate.c
 list.c
 liststore.c
  main.c
  message.c
 mrulist.c
 notebook.c
  notice.c
 popupmenu.c
 splash.c
  pulldnmenu.c
 radio.c
 recentuse.c
 resourcefile.c
 revealer.c
 scalectrl.c
 separator.c
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
 treeview.c
  wpref.c

 builder.c

  control.c
 dialog.c
 drawcairo.c

  font.c
  help.c

  opendocument.c

  tooltip.c
  util.c
 window.c
 print.c
)

if(WIN32)
    target_sources(xtrkcad-wlib
  PRIVATE
  win32/getline.c
  win32/getopt.c
  win32/sysinfo.c
  win32/w32paths.c
  win32/utf8conv.c)
endif()

if(UNIX)
    target_sources(xtrkcad-wlib
  PRIVATE
   timer.c
   print.c
   unix/ixpaths.c
   unix/sysinfo.c)
endif()

# help system is OS and build specific, add appropriate source files
if(APPLE)
    if (XTRKCAD_USE_APPLEHELP)
        target_sources(xtrkcad-wlib
   PRIVATE
   osxhelp.c)
    else()
        if(XTRKCAD_USE_BROWSER)
            target_sources(xtrkcad-wlib
    PRIVATE
              browserhelp.c)
        else()
            message(FATAL_ERROR
                "XTRKCAD_USE_BROWSER=OFF requests the built-in WebKitGTK help viewer on "
                "APPLE, but ixhelp.c (the WebKit-1.0 help viewer implementation) was "
                "removed as dead code (unreachable: webkit-1.0 has no GTK3 equivalent, "
                "same reasoning as the non-APPLE branch below). Re-run cmake with "
                "-DXTRKCAD_USE_BROWSER=ON to show help in the system's default browser "
                "instead.")
        endif()
    endif()
else()
    if(XTRKCAD_USE_BROWSER)
        target_sources(xtrkcad-wlib
   PRIVATE
   browserhelp.c)
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

# copy the glade ui definitions into the build tree
# files are copied unconditionally on every build
# add_custom_target(gladeui
#                    COMMAND ${CMAKE_COMMAND} -E copy_directory
#                     ${CMAKE_CURRENT_SOURCE_DIR}/glade
#                     ${CMAKE_BINARY_DIR}/app/bin/ui
# )
if(TARGET resource-target AND TARGET symbols-target)
	add_dependencies(xtrkcad-wlib resource-target symbols-target)
endif()

if(XTRKCAD_TESTING)
    add_subdirectory(unittests)
endif()
