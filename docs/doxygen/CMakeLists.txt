PROJECT(doxygen)

INCLUDE(FindDoxygen)

IF(WIN32)
	SET( XTrkCAD_WLIB_DIR
		"mswlib" )
ELSE(WIN32)
	SET( XTrkCAD_WLIB_DIR
		"gtklib" )
ENDIF(WIN32)

CONFIGURE_FILE(${doxygen_SOURCE_DIR}/Doxyfile.in ${doxygen_BINARY_DIR}/Doxyfile)

ADD_CUSTOM_TARGET(docs-doxygen
	COMMAND ${DOXYGEN_EXECUTABLE} ${doxygen_BINARY_DIR}/Doxyfile
	)

