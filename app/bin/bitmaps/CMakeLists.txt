# create resource file to be compiled into the program
cmake_minimum_required(VERSION 3.20)

set (RESOURCE_XML icons.gresource.xml)
set (RESOURCE_XML_IN ${RESOURCE_XML}.in)
set (ICONS_DEPENDENCIES CACHE INTERNAL "Icons dependencies")

# find the resource compiler 
find_program(RESOURCE_COMPILER NAMES glib-compile-resources REQUIRED)

#create a list of icons
if(NOT EXISTS ${RESOURCE_XML} OR (${RESOURCE_XML_IN} IS_NEWER_THAN ${RESOURCE_XML}))
    #create the XML file
    configure_file(${RESOURCE_XML}.in  ${RESOURCE_XML})

    execute_process(
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${RESOURCE_COMPILER} --generate-dependencies ${RESOURCE_XML}
        OUTPUT_VARIABLE RESOURCE_DEPENDENCIES 
    )

    # OUTPUT_VARIABLE is not a list but a single string value with newlines
    # Convert it to a list and pop out the last newline character
    string(REPLACE "\n" ";" RESOURCE_DEPENDENCIES ${RESOURCE_DEPENDENCIES})
    list(POP_BACK RESOURCE_DEPENDENCIES)
endif()

set(ICONWIDTHS 16 24 32)

#create the png files for all icons in three sizes
foreach(iconfile ${RESOURCE_DEPENDENCIES})
	foreach(pixelsize ${ICONWIDTHS})
		set(DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/png${pixelsize}")
		get_filename_component(iconfile ${iconfile} NAME_WE)
		set(PNGFILENAME "${iconfile}.png")
		set(SVGFILENAME "${iconfile}.svg")

		if ( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/png${pixelsize}/${PNGFILENAME}" )
			#message( STATUS "Custom ${DESTINATION}/${PNGFILENAME}" )
			add_custom_command(
				OUTPUT ${DESTINATION}/${PNGFILENAME}
				DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/png${pixelsize}/${PNGFILENAME}
				COMMAND ${CMAKE_COMMAND} -E make_directory ${DESTINATION}/
				COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/png${pixelsize}/${PNGFILENAME} ${DESTINATION}/${PNGFILENAME}
			)
		else()
			#message( STATUS "SVG ${CMAKE_CURRENT_SOURCE_DIR}/svg/${iconfile}.svg ${Inkscape_EXPORT}${DESTINATION}/${PNGFILENAME}" )
			add_custom_command(
				 OUTPUT ${DESTINATION}/${PNGFILENAME}
			 	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/svg/${SVGFILENAME}
			 	COMMAND ${CMAKE_COMMAND} -E make_directory ${DESTINATION}
			 	COMMAND ${Inkscape_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/svg/${SVGFILENAME} -h ${pixelsize} ${Inkscape_EXPORT}${DESTINATION}/${PNGFILENAME}
			)
		endif()
	endforeach()
endforeach()

#build one resource file per icon size
foreach(WIDTH ${ICONWIDTHS})
	list(TRANSFORM RESOURCE_DEPENDENCIES PREPEND ${CMAKE_CURRENT_BINARY_DIR}/png${WIDTH}/ OUTPUT_VARIABLE PNGDEPENDENCIES)
	set(RESOURCE_FILE icons${WIDTH}.gresource)

	#use the XML file to compile the resources into a source
	add_custom_command(
    	OUTPUT ${RESOURCE_FILE}
    	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    	COMMAND ${RESOURCE_COMPILER}
    	ARGS
        	--target=${RESOURCE_FILE}
        	--sourcedir=png${WIDTH} 
        	${CMAKE_CURRENT_BINARY_DIR}/${RESOURCE_XML}
    	VERBATIM
    	MAIN_DEPENDENCY ${RESOURCE_XML}
    	DEPENDS ${PNGDEPENDENCIES}
	)

	set(RESOURCE_FILES ${RESOURCE_FILES} ${RESOURCE_FILE})
endforeach()

# define target
add_custom_target(
  	genbitmaps
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/${RESOURCE_XML}
		${RESOURCE_FILES}
)

 