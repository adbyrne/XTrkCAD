# Setup GTK v3 UI library...

set(sources
	bitmap.c
	boxes.c
   	builder.c
	button.c
	color.c
	control.c
	droplist.c
	filesel.c
	font.c
	help.c
	lines.c
	list.c
	liststore.c
	main.c
	menu.c
	message.c
	notice.c
	opendocument.c
	pixbuf.c
	png.c
	print.c
	single.c
	splash.c
   	statusbar.c
	text.c
	timer.c
	tooltip.c
	treeview.c
	util.c
	window.c
	wpref.c
	gtkdraw-cairo.c
	)

set (SOURCES
	${SOURCES}
	dynarr.h
	gtkint.h
	uthash.h
)

# help system is OS and build specific, add appropriate source files
if(APPLE)
    set(sources
        ${sources}
	osxhelp.c)
else()
    if(XTRKCAD_USE_BROWSER)
        set(sources
            ${sources}
            browserhelp.c)
    else()
        set(sources
            ${sources}
            ixhelp.c)
    endif()
endif()

include_directories(${XTrkCAD_BINARY_DIR})

add_library(xtrkcad-wlib ${sources})

# GTK
include_directories(${GTK_INCLUDE_DIRS})
target_link_libraries(xtrkcad-wlib PRIVATE ${GTK_LIBRARIES})

# configure for GTK's native Unix print
unset(GTK_UNIX_PRINT_FOUND CACHE)
find_package (GTKUnixPrint)
include_directories(${GTK_UNIX_PRINT_INCLUDE_DIRS})
target_link_libraries(xtrkcad-wlib PRIVATE ${GTK_UNIX_PRINT_LIBRARIES})

# add dependency to webkit if configured
if(NOT XTRKCAD_USE_BROWSER)
    include_directories(${GTK_WEBKIT_INCLUDE_DIRS})
    target_link_libraries(xtrkcad-wlib PRIVATE ${GTK_WEBKIT_LIBRARIES})
endif()

# WrapBox
target_include_directories(xtrkcad-wlib PRIVATE ${CMAKE_CURRENT_LIST_DIR}/wrapbox/)
target_link_libraries(xtrkcad-wlib PRIVATE wrapbox)
add_subdirectory(wrapbox)

# copy the glade ui definitions into the build tree

add_custom_target(gladeui
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_CURRENT_SOURCE_DIR}/glade
                    ${CMAKE_BINARY_DIR}/app/bin/ui
)
add_dependencies(xtrkcad-wlib gladeui)

install(DIRECTORY glade/ DESTINATION bin/ui/)
