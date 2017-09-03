
file(GLOB headers *.h)

find_package (GTKUnixPrint)

set(sources
	bitmap.c
	boxes.c
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
	pixbuf.c
	png.c
	print.c
	single.c
	splash.c
	text.c
	timer.c
	tooltip.c	
	treeview.c
	util.c
	window.c
	wpref.c
# end of refactored sources	
	gtkdraw-cairo.c
	)

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
include_directories(${GTK_INCLUDE_DIRS})
include_directories(${GTK_UNIX_PRINT_INCLUDE_DIRS})

add_library(xtrkcad-wlib ${headers} ${sources})
target_link_libraries(xtrkcad-wlib ${GTK_LIBRARIES})
target_link_libraries(xtrkcad-wlib ${GTK_UNIX_PRINT_LIBRARIES})

include_directories(${GTK_WEBKIT_INCLUDE_DIRS})
target_link_libraries(xtrkcad-wlib ${GTK_WEBKIT_LIBRARIES})

