project(sourcedoc C)

#
# create source documentation using Doxygen

find_package(Doxygen
 REQUIRED
)

SET( XTrkCAD_WLIB_DIR "gtk3lib" )

configure_file(${sourcedoc_SOURCE_DIR}/Doxyfile.in ${sourcedoc_BINARY_DIR}/Doxyfile)

add_custom_target(docs-doxygen
 COMMAND ${DOXYGEN_EXECUTABLE} ${sourcedoc_BINARY_DIR}/Doxyfile
 )
