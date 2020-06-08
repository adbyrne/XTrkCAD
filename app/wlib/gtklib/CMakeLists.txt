# Setup GTK UI library...
file(GLOB headers *.h)

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
# end of refactored sources	
	gtkdraw-cairo.c
	)

# help system is OS and build specific, add appropriate source files
if(APPLE)
   if (XTRKCAD_USE_APPLEHELP) 
   		set(sources 
      	  ${sources}
			osxhelp.c)	
	else(XTRKCAD_USE_APPLEHELP)
		if(XTRKCAD_USE_BROWSER) 
        	set(sources 
          	  ${sources}
          	  browserhelp.c)
    	else(XTRKCAD_USE_BROWSER)
       	 set(sources 
       	     ${sources}
       	     ixhelp.c)
       	endif(XTRKCAD_USE_BROWSER)
    endif(XTRKCAD_USE_APPLEHELP)    
else(APPLE)
    if(XTRKCAD_USE_BROWSER) 
        set(sources 
            ${sources}
            browserhelp.c)
    else(XTRKCAD_USE_BROWSER)
        set(sources 
            ${sources}
            ixhelp.c)
    endif(XTRKCAD_USE_BROWSER)    
endif(APPLE)

include_directories(${XTrkCAD_BINARY_DIR})

add_library(xtrkcad-wlib ${headers} ${sources})

# GTK 
find_package (GTK2)
include_directories(${GTK_INCLUDE_DIRS})
target_link_libraries(xtrkcad-wlib ${GTK_LIBRARIES})

# configure for GTK's native Unix print
find_package (GTKUnixPrint)
include_directories(${GTK_UNIX_PRINT_INCLUDE_DIRS})
target_link_libraries(xtrkcad-wlib ${GTK_UNIX_PRINT_LIBRARIES})

# add dependency to webkit if configured
if(NOT XTRKCAD_USE_BROWSER)
    include_directories(${GTK_WEBKIT_INCLUDE_DIRS})
    target_link_libraries(xtrkcad-wlib ${GTK_WEBKIT_LIBRARIES})
endif()
