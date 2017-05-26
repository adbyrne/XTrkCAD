# CMakeList for the LIB directory
#
# Include the subdirectories for params, examples and params
# Generate and install a Readme.txt file from markdown if pandoc is available on the system
# install desktop integration following freedesktop.org specification 
#
project(lib)

add_subdirectory(demos)
add_subdirectory(examples)
add_subdirectory(params)

install(FILES 
	COPYING
	logo.bmp
	xtrkcad.xtq
	DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}
	)
	
set( infile "${CMAKE_CURRENT_SOURCE_DIR}/Readme.md" )
set( outfile "${CMAKE_CURRENT_BINARY_DIR}/Readme.txt" )

if(PANDOC_FOUND)
	add_custom_command(OUTPUT "${outfile}"
					   COMMAND ${PANDOC_EXECUTABLE} "--from=Markdown" "--to=plain" "-o" ${outfile} ${infile} 
					   DEPENDS "${infile}" 
					  )
	add_custom_target(Readme ALL DEPENDS ${outfile})					  
	install(FILES ${outfile}
			DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}
			)
else()			
	message(STATUS "Pandoc is not available on this system, Readme.txt is not generated!")
endif()
	
if(UNIX AND NOT APPLE)
	install(FILES
		xtrkcad.desktop
		DESTINATION "/usr/share/applications"
		)
	install(FILES
		xtrkcad.png
		DESTINATION "/usr/share/pixmaps"
		)
endif()