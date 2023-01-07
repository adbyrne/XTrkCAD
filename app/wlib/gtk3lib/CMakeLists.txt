# Setup GTK3 UI library...

add_subdirectory(glade)
add_subdirectory(wrapbox)

target_include_directories(xtrkcad-wlib 
	PRIVATE 
	${xtrkcad-lib_SOURCE_DIR}
)

target_sources(xtrkcad-wlib
	PRIVATE 
	basicdraw.c
	bitmap.c
 	boxes.c
	builder.c
 	button.c
 	color.c
 	control.c
	drawcairo.c
	droplist.c	
 	filesel.c
 	font.c
 	help.c
 	lines.c
 	list.c
 	liststore.c
 	main.c
 	message.c
 	notice.c
 	opendocument.c
 	pixbuf.c
 	print.c
 	pulldnmenu.c	
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

# GTK 
find_package (GTK3 REQUIRED)
# configure for GTK's native Unix print
find_package (GTKUnixPrint3 REQUIRED)

target_include_directories(xtrkcad-wlib
	PRIVATE
	${GTK3_INCLUDE_DIRS}
	${GTK_UNIX_PRINT3_INCLUDE_DIRS}
)
target_link_libraries(xtrkcad-wlib 
	PRIVATE
	dynstring
	wrapbox
	${GTK_UNIX_PRINT3_LIBRARIES}
	${GTK3_LIBRARIES}
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

# copy the glade ui definitions into the build tree
# files are copied unconditionally on every build
# add_custom_target(gladeui
#                    COMMAND ${CMAKE_COMMAND} -E copy_directory
#                     ${CMAKE_CURRENT_SOURCE_DIR}/glade
#                     ${CMAKE_BINARY_DIR}/app/bin/ui
# )
add_dependencies(xtrkcad-wlib uifiles)

