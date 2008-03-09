# FILE(GLOB C_FILES *.c)
# FILE(GLOB CAR_FILES *.car)
# FILE(GLOB STRUCT_FILES *.struct)
# FILE(GLOB TAB_FILES *.tab)
# FILE(GLOB TO_FILES *.to)
FILE(GLOB XTP_FILES *.xtp)

ADD_EXECUTABLE( mkstruct mkstruct.c )

GET_TARGET_PROPERTY(mkstruct_EXE mkstruct LOCATION)

SET( STRUCT_SOURCES 
	 ${CMAKE_CURRENT_SOURCE_DIR}/fallerho.struct
	 ${CMAKE_CURRENT_SOURCE_DIR}/pikestuf.struct
	 ${CMAKE_CURRENT_SOURCE_DIR}/revell.struct
	 ${CMAKE_CURRENT_SOURCE_DIR}/smltown.struct )
	 
ADD_CUSTOM_TARGET( xtpfiles
				   DEPENDS STRUCT_SOURCES mkstruct
				   COMMAND ${mkstruct_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/fallerho.struct ${CMAKE_CURRENT_BINARY_DIR}/fallerho.xtp 
				   COMMAND ${mkstruct_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/pikestuf.struct ${CMAKE_CURRENT_BINARY_DIR}/pikestuf.xtp 			   			   
				   COMMAND ${mkstruct_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/revell.struct ${CMAKE_CURRENT_BINARY_DIR}/revell.xtp 			   			   				   
				   COMMAND ${mkstruct_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/smltown.struct ${CMAKE_CURRENT_BINARY_DIR}/smltown.xtp 			   			   				   
				   )

INSTALL(
#	FILES ${C_FILES} ${CAR_FILES} ${STRUCT_FILES} ${TAB_FILES} ${TO_FILES} ${XTP_FILES}
	FILES ${XTP_FILES}
	DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}/params
	)

