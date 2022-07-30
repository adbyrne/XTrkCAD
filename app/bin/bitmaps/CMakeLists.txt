# Generate XPM bitmaps from SVG files

PROJECT(genbitmaps)

find_program( SVG_CONVERTER
	NAMES inkscape.exe inkscape rsvg-convert
#	REQUIRED
	HINTS "C:/Program Files/Inkscape/bin" "/usr/bin"
	)

#	ease-broad ease-cornu ease-gt-broad ease-gt-sharp ease-lt-broad 
#	ease-lt-sharp ease-none ease-normal ease-sharp 

if( ${SVG_CONVERTER} STREQUAL "SVG_CONVERTER-NOTFOUND" )
	message( STATUS "SVG Converter not found." )
else()
	SET (XPMS
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

	if (WIN32)
		set( OUTOPT
			-o
		)
	else(WIN32)
		set( OUTOPT
			-e
		)
	endif(WIN32)

	foreach(xpm ${XPMS})
		add_custom_command(
			OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${xpm}.xpm
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/SVG/${xpm}.svg
			COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/PNG/
			COMMAND ${SVG_CONVERTER} ${CMAKE_CURRENT_SOURCE_DIR}/SVG/${xpm}.svg -h 16 ${OUTOPT} ${CMAKE_CURRENT_SOURCE_DIR}/PNG/${xpm}16.png
			COMMAND ${SVG_CONVERTER} ${CMAKE_CURRENT_SOURCE_DIR}/SVG/${xpm}.svg -h 24 ${OUTOPT} ${CMAKE_CURRENT_SOURCE_DIR}/PNG/${xpm}24.png
			COMMAND ${SVG_CONVERTER} ${CMAKE_CURRENT_SOURCE_DIR}/SVG/${xpm}.svg -h 32 ${OUTOPT} ${CMAKE_CURRENT_SOURCE_DIR}/PNG/${xpm}32.png
			COMMAND pngtoxpm ${CMAKE_CURRENT_SOURCE_DIR}/${xpm}.xpm	
			COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_CURRENT_SOURCE_DIR}/PNG/${xpm}16.png
			COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_CURRENT_SOURCE_DIR}/PNG/${xpm}24.png
			COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_CURRENT_SOURCE_DIR}/PNG/${xpm}32.png
		)
		# build a list of all the depends
		list(APPEND XPM_RESULTS ${xpm}.xpm)
	endforeach()

	add_custom_target (
		genbitmaps 
		DEPENDS ${XPM_RESULTS}
	)
endif()

MESSAGE (STATUS "SVG Converter: ${SVG_CONVERTER}")
# MESSAGE (STATUS "Source Dir: ${CMAKE_CURRENT_SOURCE_DIR}")
# MESSAGE (STATUS "XPMs: ${XPMS}")
# MESSAGE (STATUS "XPM_RESULTS: ${XPM_RESULTS}")
