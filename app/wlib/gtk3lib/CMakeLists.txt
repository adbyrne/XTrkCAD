# Setup GTK3 UI library...

add_subdirectory(ui)

target_include_directories(xtrkcad-wlib 
	PRIVATE 
	${xtrkcad-lib_SOURCE_DIR}
)

target_sources(xtrkcad-wlib
	PRIVATE 
	appwindow.c
	basicdraw.c
	bitmap.c
	boxes.c
	button.c
	color.c
	combobox.c	
	entry.c
 	fileselector.c
 	lines.c
	list.c
	liststore.c
 	main.c
 	message.c
 	notice.c
	splash.c
 	pulldnmenu.c
	radio.c
	recentuse.c
	statusbar.c
 	text.c
	toggle.c
	treeview.c
 	wpref.c

	builder.c

 	control.c
	dialog.c
	drawcairo.c

 	font.c
 	help.c

 	opendocument.c
 	pixbuf.c

 	tooltip.c	
 	util.c
 	window.c
)

if(WIN32)
	target_sources(xtrkcad-wlib
		PRIVATE
		win32/w32paths.c)
endif()

if(UNIX)
	target_sources(xtrkcad-wlib
		PRIVATE
			timer.c
			print.c 
			unix/ixpaths.c)
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

target_include_directories(xtrkcad-wlib
	PRIVATE
	${GTK3_INCLUDE_DIRS}
)

message(STATUS "Include Dirs: ${GTK3_INCLUDE_DIRS}")

target_link_libraries(xtrkcad-wlib 
	PRIVATE
	dynstring
	${GTK3_LIBRARIES}
)

message(STATUS "Libraries: ${GTK3_LIBRARIES}")

# configure for GTK's native Unix print
if(UNIX)
	find_package (GTKUnixPrint3 REQUIRED)
	
	target_include_directories(xtrkcad-wlib
		PRIVATE
		${GTK_UNIX_PRINT3_INCLUDE_DIRS}
	)

	target_link_libraries(xtrkcad-wlib 
		PRIVATE
		${GTK_UNIX_PRINT3_LIBRARIES}
	)
endif()

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
add_dependencies(xtrkcad-wlib resource-target)

add_subdirectory(unittests)