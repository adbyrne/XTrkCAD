# Generate XPM bitmaps from SVG files

PROJECT(genbitmaps)

find_program(SVG_CONVERTER
	NAMES inkscape.exe inkscape rsvg-convert
	REQUIRED
	HINTS "C:\\Program Files\\Inkscape\\bin" "/usr/bin")

SET (XPMS
	box
	doc-export-svg
    )

foreach(xpm ${XPMS})
	add_custom_command(
		OUTPUT ${xpm}.xpm
		COMMAND echo ${xpm} ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND ${SVG_CONVERTER} ${CMAKE_CURRENT_SOURCE_DIR}/svg/${xpm}.svg -h 16 -o ${CMAKE_CURRENT_SOURCE_DIR}/png/${xpm}16.png
		COMMAND ${SVG_CONVERTER} ${CMAKE_CURRENT_SOURCE_DIR}/svg/${xpm}.svg -h 24 -o ${CMAKE_CURRENT_SOURCE_DIR}/png/${xpm}24.png
		COMMAND ${SVG_CONVERTER} ${CMAKE_CURRENT_SOURCE_DIR}/svg/${xpm}.svg -h 32 -o ${CMAKE_CURRENT_SOURCE_DIR}/png/${xpm}32.png
		COMMAND ${CMAKE_CURRENT_BINARY_DIR}/pngtoxpm ${CMAKE_CURRENT_SOURCE_DIR}/${xpm}
	)
	
	# build a list of all the results
	list(APPEND XPM_RESULTS ${PROJECT_SOURCE_DIR}/${XPM}.xpm)
endforeach()

add_custom_target (
	bitmaps ALL
	DEPENDS ${XPM_RESULTS}
)
