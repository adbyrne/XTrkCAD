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
		ease-broad ease-cornu ease-gt-broad ease-gt-sharp ease-lt-broad 
		ease-lt-sharp ease-none ease-normal ease-sharp
		elevation exit extend filled-box filled-polygon go helix
		info join-line join layers magnet manage map move
		new-car ok pan-zoom parallel-line parallel parameter paste
		polygon polyline profile protractor redo reflect roadbed
		rotate ruler select sensor signal snap-curs snap-grid
		split-draw split sticky-doc sticky-link sticky-note stop
		straight-line straight switch-motor table-edge text ties top
		train trim tunnel turnout-design turnout turntable undo
		zoom-choose zoom-extent zoom-in zoom-out
	)

	set( PIXS 16 24 32 )

	foreach(xpm ${XPMS})
		foreach(pix ${PIXS})
			if ( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/png/${xpm}${pix}.png" )
				# MESSAGE( STATUS "Custom ${CMAKE_CURRENT_SOURCE_DIR}/png/${xpm}${pix}.png" )
				add_custom_command(
					OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/png/${xpm}${pix}.png
					DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/png/${xpm}${pix}.png
					COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/png/
					COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/png/${xpm}${pix}.png ${CMAKE_CURRENT_BINARY_DIR}/png/${xpm}${pix}.png
				)
			else()
				# MESSAGE( STATUS "SVG ${CMAKE_CURRENT_SOURCE_DIR}/svg/${xpm}.svg" )
				add_custom_command(
					OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/png/${xpm}${pix}.png
					DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/svg/${xpm}.svg
					COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/png/
					COMMAND ${Inkscape_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/svg/${xpm}.svg -h ${pix} "${Inkscape_EXPORT}=${CMAKE_CURRENT_BINARY_DIR}/png/${xpm}${pix}.png"
				)
			endif()
			# build a list of all the depends
			list(APPEND PNG_RESULTS ${CMAKE_CURRENT_BINARY_DIR}/png/${xpm}${pix}.png)
		endforeach()
		add_custom_command(
			OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${xpm}.xpm3
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/png/${xpm}16.png
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/png/${xpm}24.png
			DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/png/${xpm}32.png
			COMMAND pngtoxpm ${CMAKE_CURRENT_BINARY_DIR}/${xpm}.xpm3	
#			COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/${xpm}.xpm3 ${CMAKE_CURRENT_SOURCE_DIR}/${xpm}.xpm3 
		)
		# build a list of all the depends
		list(APPEND XPM_RESULTS ${xpm}.xpm3)
	endforeach()

	add_custom_target (
		genbitmaps 
		DEPENDS ${XPM_RESULTS}
		DEPENDS ${PNG_RESULTS}
	)
endif()

#MESSAGE (STATUS "SVG Converter: ${SVG_CONVERTER}")
#MESSAGE (STATUS "Source Dir: ${CMAKE_CURRENT_SOURCE_DIR}")
#MESSAGE (STATUS "XPMs: ${XPMS}")
#MESSAGE (STATUS "XPM_RESULTS: ${XPM_RESULTS}")
#MESSAGE (STATUS "PNG_RESULTS: ${PNG_RESULTS}")
