# Setup GTK UI library...

target_include_directories(xtrkcad-wlib 
	PRIVATE 
	${xtrkcad-lib_SOURCE_DIR}
)

target_sources(xtrkcad-wlib
	PRIVATE 
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
	writebitmap.c
	gtkdraw-cairo.c
)

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
    		PKG_CHECK_MODULES(GTK_WEBKIT "webkit-1.0" REQUIRED)
        	target_sources(xtrkcad-wlib
				PRIVATE
       	     	ixhelp.c)
       	endif()
    endif()    
else()
    if(XTRKCAD_USE_BROWSER) 
		target_sources(xtrkcad-wlib
			PRIVATE
			browserhelp.c)
    else()
    	PKG_CHECK_MODULES(GTK_WEBKIT "webkit-1.0" REQUIRED)
			target_sources(xtrkcad-wlib
				PRIVATE
    	        ixhelp.c)
    endif()    
endif()

# WrapBox
target_include_directories(xtrkcad-wlib PRIVATE ${CMAKE_CURRENT_LIST_DIR}/wrapbox/)
TARGET_LINK_LIBRARIES(xtrkcad-wlib PRIVATE wrapbox)


# GTK 
find_package (GTK2)

# configure for GTK's native Unix print
unset(GTK_UNIX_PRINT_FOUND CACHE)
find_package (GTKUnixPrint)

target_include_directories(xtrkcad-wlib
	PRIVATE
	${GTK_INCLUDE_DIRS}
	${GTK_UNIX_PRINT_INCLUDE_DIRS}
)
target_link_libraries(xtrkcad-wlib 
	PRIVATE
	dynstring
	${GTK_UNIX_PRINT_LIBRARIES}
	${GTK_LIBRARIES}
)

# add dependency to webkit if configured
if (APPLE)
	if(NOT XTRKCAD_USE_APPLEHELP) 
		if(NOT XTRKCAD_USE_BROWSER)
			target_include_directories(xtrkcad-wlib
				${GTK_WEBKIT_INCLUDE_DIRS}
			)
	 	   target_link_libraries(xtrkcad-wlib 
				PRIVATE
				${GTK_WEBKIT_LIBRARIES}
			)
		endif()
	endif() 
else ()
	if(NOT XTRKCAD_USE_BROWSER)
 	   target_include_directories(xtrkcad-wlib
			${GTK_WEBKIT_INCLUDE_DIRS}
		)
 	   target_link_libraries(xtrkcad-wlib 
			PRIVATE
			${GTK_WEBKIT_LIBRARIES}
		)
	endif()
endif()
