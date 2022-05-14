
# CMakeList for the params directory
#
# Several xtp files are created from text definitions, the created xtp files are installed.
#

add_custom_target(xtpfiles ALL "")
set_target_properties(
   xtpfiles
   PROPERTIES FOLDER "Param Files"
)

#
#  create param files from car definitions

add_executable( mkcarpart mkcarpart.c )
set_target_properties(
   mkcarpart
   PROPERTIES FOLDER "Param Files"
)

set(infiles
    "AccucraftFn3.cars"
    "AccucraftOn3.cars"
    "accurail.cars"
    "atlascho.cars"
    "atlaseho.cars"
)

foreach(infile ${infiles})
    # Generate output file name
    cmake_path(REPLACE_EXTENSION infile
                "xtp" 
                OUTPUT_VARIABLE outfile
    )
    # Custom command to do the processing
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${outfile}"
        DEPENDS "${infile}" mkcarpart 
        COMMAND mkcarpart "${infile}" "${CMAKE_CURRENT_BINARY_DIR}/${outfile}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" 
        )
    target_sources(xtpfiles
        PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/${outfile}"
    )
    install(
	    FILES "${CMAKE_CURRENT_BINARY_DIR}/${outfile}"
	    DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}/params
	)
endforeach()

# create param files from  structure definitions

add_executable( mkstruct mkstruct.c )
set_target_properties(
   mkstruct
   PROPERTIES FOLDER "Param Files"
)

set(infiles
    "fallerho.struct"
    "revell.struct"
    "pikestuf.struct"
)

# define build commands for all struct files
foreach(infile ${infiles})
    # Generate output file name
    cmake_path(REPLACE_EXTENSION infile
                "xtp" 
                OUTPUT_VARIABLE outfile
    )
    cmake_path(ABSOLUTE_PATH 
                outfile
                BASE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" )

    add_custom_command(
        OUTPUT "${outfile}"
        COMMAND mkstruct "${infile}" "${outfile}"
        DEPENDS "${infile}" mkstruct
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" )

    target_sources(xtpfiles
       PRIVATE
       "${outfile}"
    )
    install(
        FILES "${outfile}"
        DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}/params
)
endforeach()

install(
	FILES t-trak-notes.txt
	DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}/params
	)
