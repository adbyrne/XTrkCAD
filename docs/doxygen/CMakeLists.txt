project(sourcedoc)

#
# create source documentation using Doxygen 

find_package(Doxygen
	REQUIRED
)

if(XTRKCAD_GTK3_WLIB)
	SET( XTrkCAD_WLIB_DIR
	"gtk3lib" )	
else()
	if(WIN32)
		set( XTrkCAD_WLIB_DIR
			"mswlib" )
	else(WIN32)
		SET( XTrkCAD_WLIB_DIR
			"gtklib" )
	endif(WIN32)
endif()

configure_file(${sourcedoc_SOURCE_DIR}/Doxyfile.in ${sourcedoc_BINARY_DIR}/Doxyfile)

add_custom_target(docs-doxygen
	COMMAND ${DOXYGEN_EXECUTABLE} ${sourcedoc_BINARY_DIR}/Doxyfile
	)

