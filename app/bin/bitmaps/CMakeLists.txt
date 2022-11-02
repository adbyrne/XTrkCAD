# Generate XPM bitmaps from SVG files

PROJECT(genbitmaps)

# Find the conversion tool 

find_package(Inkscape)

if( NOT Inkscape_FOUND )
	message( STATUS "Inkscape not found." )
else()
	message(STATUS "Inkscape is: ${Inkscape_EXECUTABLE}")
	message(STATUS "Export option is: ${Inkscape_EXPORT}")

	set (XPMS
		background benchwork bezier-line bezier-track block bottom box
		bridge building car-inventory change-grid circle-center
		circle-filled-center circle-filled-tangent circle-line-center
		circle-line-tangent circle-line circle-tangent circle clone
		connect control convert-from convert-to copy cornu curved-chord
		curved-end curved-line-chord curved-line-end curved-line-middle
		curved-line-tangent curved-middle curved-tangent cut delete
		describe description dimension doc-export-bmap doc-export-dxf
		doc-export-svg doc-export doc-import-mod doc-import doc-new doc-open 
		doc-print doc-recent doc-revert doc-save-as doc-save doc-setup down 
		elevation exit extend filled-box filled-polygon go helix
		Info join-line join layers magnet manage map move
		new-car ok pan-zoom parallel-line parallel parameter paste
		polygon polyline profile protractor redo reflect roadbed
		rotate ruler select sensor signal snap-curs snap-grid
		split-draw split sticky-doc sticky-link sticky-note stop
		straight-line straight switch-motor table-edge text ties top
		train trim tunnel turnout-design turnout turntable undo
		zoom-choose zoom-extent zoom-in zoom-out
	)

	foreach(xpm ${XPMS})
		add_custom_command(
			OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${xpm}.xpm
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/SVG/${xpm}.svg
			COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/PNG/
			COMMAND ${Inkscape_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/SVG/${xpm}.svg -h 16 "${Inkscape_EXPORT}=${CMAKE_CURRENT_BINARY_DIR}/PNG/${xpm}16.png"
			COMMAND ${Inkscape_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/SVG/${xpm}.svg -h 24 "${Inkscape_EXPORT}=${CMAKE_CURRENT_BINARY_DIR}/PNG/${xpm}24.png"
			COMMAND ${Inkscape_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/SVG/${xpm}.svg -h 32 "${Inkscape_EXPORT}=${CMAKE_CURRENT_BINARY_DIR}/PNG/${xpm}32.png"
			COMMAND pngtoxpm ${CMAKE_CURRENT_BINARY_DIR}/${xpm}.xpm	
#			COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/${xpm}.xpm ${CMAKE_CURRENT_SOURCE_DIR}/${xpm}.xpm 
		)
		# build a list of all the depends
		list(APPEND XPM_RESULTS ${xpm}.xpm)
	endforeach()

	add_custom_target (
		genbitmaps 
		DEPENDS ${XPM_RESULTS}
	)
endif()
