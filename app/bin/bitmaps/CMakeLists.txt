# Generate XPM bitmaps from SVG files

PROJECT(genbitmaps)

find_program( SVG_CONVERTER
	NAMES inkscape.exe inkscape rsvg-convert
	REQUIRED
	HINTS "C:\\Program Files\\Inkscape\\bin" "/usr/bin"
	)

SET (XPMS
	box
	circle
	doc-export-svg
    )

foreach(xpm ${XPMS})
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${xpm}.xpm
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/SVG/${xpm}.svg
		MESSAGE (STATUS "${xpm} ${CMAKE_CURRENT_SOURCE_DIR}")
		COMMAND ${SVG_CONVERTER} ${CMAKE_CURRENT_SOURCE_DIR}/svg/${xpm}.svg -h 16 -o ${CMAKE_CURRENT_SOURCE_DIR}/png/${xpm}16.png
		COMMAND ${SVG_CONVERTER} ${CMAKE_CURRENT_SOURCE_DIR}/svg/${xpm}.svg -h 24 -o ${CMAKE_CURRENT_SOURCE_DIR}/png/${xpm}24.png
		COMMAND ${SVG_CONVERTER} ${CMAKE_CURRENT_SOURCE_DIR}/svg/${xpm}.svg -h 32 -o ${CMAKE_CURRENT_SOURCE_DIR}/png/${xpm}32.png
		COMMAND pngtoxpm ${CMAKE_CURRENT_SOURCE_DIR}/${xpm}.xpm
	)
	
	# build a list of all the results
	list(APPEND XPM_RESULTS ${xpm}.xpm)
endforeach()

add_custom_target (
	genbitmaps ALL
	DEPENDS ${XPM_RESULTS}
)

MESSAGE (STATUS "SVG Converter: ${SVG_CONVERTER}")
MESSAGE (STATUS "Source Dir: ${CMAKE_CURRENT_SOURCE_DIR}")
MESSAGE (STATUS "XPM_RESULTS: ${XPM_RESULTS}")

