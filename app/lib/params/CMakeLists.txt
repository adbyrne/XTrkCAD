# FILE(GLOB C_FILES *.c)
# FILE(GLOB CAR_FILES *.car)
# FILE(GLOB STRUCT_FILES *.struct)
# FILE(GLOB TAB_FILES *.tab)
# FILE(GLOB TO_FILES *.to)
FILE(GLOB XTP_FILES *.xtp)

SET( outFiles )
#
#  create param files from car definitions
#

ADD_EXECUTABLE( mkcarpart mkcarpart.c )
GET_TARGET_PROPERTY(mkcarpart_EXE mkcarpart LOCATION)

FILE(GLOB inFiles RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*.cars")
	 
FOREACH(infileName ${inFiles})
    # Generate output file name
    STRING(REGEX REPLACE ".cars\$" ".xtp" outfileName "${infileName}")
    SET(outfile "${CMAKE_CURRENT_BINARY_DIR}/${outfileName}")
    # Generate input file name
    SET(infile "${CMAKE_CURRENT_SOURCE_DIR}/${infileName}")
    # Custom command to do the processing
    ADD_CUSTOM_COMMAND(OUTPUT "${outfile}"
        COMMAND ${mkcarpart_EXE} "${infile}" "${outfile}"
        DEPENDS "${infile}" mkcarpart
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "create car definitions")
    # Finally remember the output file for dependencies
    SET(outFiles ${outFiles} "${outfile}")
ENDFOREACH(infileName)

#
# create param files from  structure definitions
#

# create the tool
ADD_EXECUTABLE( mkstruct mkstruct.c )
GET_TARGET_PROPERTY(mkstruct_EXE mkstruct LOCATION)

# find all struct files
FILE(GLOB inFiles RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*.struct")

# define build commands for all struct files
FOREACH(infileName ${inFiles})
    # Generate output file name
    STRING(REGEX REPLACE ".struct\$" ".xtp" outfileName "${infileName}")
    SET(outfile "${CMAKE_CURRENT_BINARY_DIR}/${outfileName}")
    # Generate input file name
    SET(infile "${CMAKE_CURRENT_SOURCE_DIR}/${infileName}")
    # Custom command to do the processing
    ADD_CUSTOM_COMMAND(OUTPUT "${outfile}"
        COMMAND ${mkstruct_EXE} "${infile}" "${outfile}"
        DEPENDS "${infile}" mkstruct
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Convert structure definitions ${infileName}")
    # Finally remember the output file for dependencies
    SET(outFiles ${outFiles} "${outfile}")
ENDFOREACH(infileName)

ADD_CUSTOM_TARGET(xtpfiles ALL DEPENDS ${outFiles})
	 
#ADD_CUSTOM_TARGET( xtpfiles
#				   DEPENDS STRUCT_SOURCES mkstruct
#				   COMMAND ${mkstruct_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/fallerho.struct ${CMAKE_CURRENT_BINARY_DIR}/fallerho.xtp 
#				   COMMAND ${mkstruct_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/pikestuf.struct ${CMAKE_CURRENT_BINARY_DIR}/pikestuf.xtp 			   			   
#				   COMMAND ${mkstruct_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/revell.struct ${CMAKE_CURRENT_BINARY_DIR}/revell.xtp 			   			   				   
#				   COMMAND ${mkstruct_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/smltown.struct ${CMAKE_CURRENT_BINARY_DIR}/smltown.xtp 			   			   				   
#				   )

INSTALL(
#	FILES ${C_FILES} ${CAR_FILES} ${STRUCT_FILES} ${TAB_FILES} ${TO_FILES} ${XTP_FILES}
	FILES ${XTP_FILES} ${outFiles}
	DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}/params
	)

