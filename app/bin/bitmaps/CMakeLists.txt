# Generate bitmaps from SVG files

PROJECT(genbitmaps)

# If ../../../bitmaps exists it is because we are building for FlatPak which can't
# build bitmaps because we don't have inkscape and mogrifiy
# Instead we copy the bitmaps from a regular build and include them at the top
# level of the source tree in the tarball we create for the FlatPak build
# See distribution/flatpak/make-source-archive
# So if bitmaps/ exists then we are doing a FlatPak build

if ( NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../bitmaps/" )
	# NOT FLATPAK
	# Find the conversion tool 
	find_package(Inkscape)

	if( NOT Inkscape_FOUND )
		message( STATUS "Inkscape not found." )
		return()
	else()
		message(STATUS "Inkscape is: ${Inkscape_EXECUTABLE}")
		message(STATUS "Export option is: ${Inkscape_EXPORT}")
	endif()
endif()

set (IMG3S
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

foreach(img3 ${IMG3S})
	if ( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../bitmaps/" )
		# FlatPak
		# Copy .image3 from generated bitmaps/ 
		if ( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../bitmaps/${img3}.image3" )
			add_custom_command(
				OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${img3}.image3
				DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../../bitmaps/${img3}.image3
				COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/../../../bitmaps/${img3}.image3 ${CMAKE_CURRENT_BINARY_DIR}/${img3}.image3
			)
		else()
			MESSAGE( STATUS "Missing ${CMAKE_CURRENT_SOURCE_DIR}/../../../bitmaps/${img3}.png3" )
		endif()
	else()
		foreach(pix ${PIXS})
			if ( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/png/${img3}${pix}.png" )
				# MESSAGE( STATUS "Custom ${CMAKE_CURRENT_SOURCE_DIR}/png/${img3}${pix}.png" )
				add_custom_command(
					OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}${pix}.png
					DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/png/${img3}${pix}.png
					COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/png/
					COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/png/${img3}${pix}.png ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}${pix}.png
				)
			else()
				# MESSAGE( STATUS "SVG ${CMAKE_CURRENT_SOURCE_DIR}/svg/${img3}.svg" )
				add_custom_command(
					OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}${pix}.png
					DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/svg/${img3}.svg
					COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/png/
					COMMAND ${Inkscape_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/svg/${img3}.svg -h ${pix} ${Inkscape_EXPORT}${CMAKE_CURRENT_BINARY_DIR}/png/${img3}${pix}.png
				)
			endif()
			# build a list of all the depends
			list(APPEND PNG_RESULTS ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}${pix}.png)
		endforeach()
		if(UNIX)
			add_custom_command(
				DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}16.png
				DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}24.png
				DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}32.png
				OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${img3}.image3
				COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/mkimage3  ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}16.png	
			)
		else()
			add_custom_command(
				DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}16.png
				DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}24.png
				DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}32.png
				OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${img3}.image3
				OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${img3}.xpm3
				COMMAND pngtoxpm ${CMAKE_CURRENT_BINARY_DIR}/${img3}.xpm3
				COMMAND -E echo "#include \"bitmaps\/${img3}.xpm3" > ${CMAKE_CURRENT_BINARY_DIR}/${img3}.image3
			)
		endif()
	endif()

	# build a list of all the depends
	if(UNIX)
		list(APPEND IMG_RESULTS ${img3}.image3)
	else()
		list(APPEND IMG_RESULTS ${img3}.xpm3)
	endif()
endforeach()

if(WIN32)
	# Temporarily the FreeImage dll is copied to the build directory
	add_custom_command(
		TARGET genbitmaps PRE_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy ${FREEIMAGE_SHAREDLIB} ${CMAKE_CURRENT_BINARY_DIR}
	)
	cmake_path(GET FREEIMAGE_SHAREDLIB FILENAME filename)
	add_custom_command(
		TARGET genbitmaps POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E rm ${CMAKE_CURRENT_BINARY_DIR}/${filename}
	)

endif()

set (IMG1S
	ballgreen ballred bluedot bo_edge bo_flat bo_lld bo_lli bo_llu bo_ll bo_lrd bo_lri
	bo_lru bo_lr bo_ti bo_tl bo_tr bo_t carpart carproto funnelclear funnel greendot
	greenstar greydot greystar loosen note partlist pause reddot redstar struct xtc
	yellowdot yellowstar zero
)
foreach(img1 ${IMG1S})
	if ( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../bitmaps/" )
		# FlatPak
		# Copy .image1 from generated bitmaps/ 
		if ( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../bitmaps/${img1}.image1" )
			add_custom_command(
				OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${img1}.image1
				DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../../bitmaps/${img1}.image1
				COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/../../../bitmaps/${img1}.image1 ${CMAKE_CURRENT_BINARY_DIR}/${img1}.image1
			)
		else()
			MESSAGE( STATUS "Missing ${CMAKE_CURRENT_SOURCE_DIR}/../../../bitmaps/${img1}.image1" )
		endif()
	else()
		if ( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${img1}.png" )
			if(UNIX)
				add_custom_command(
					OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${img1}.image1
					DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${img1}.png
					COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/
					COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/mkimage1 ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${img1}.png
				)
			else()
				add_custom_command(
					OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${img1}.image1
					DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${img1}.png
					COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/
					COMMAND -E echo "#include \"bitmaps\/${img1}.xpm" > ${CMAKE_CURRENT_BINARY_DIR}/${img1}.image1
				)
			endif()
		else()
			MESSAGE( STATUS "${CMAKE_CURRENT_SOURCE_DIR}/${img1}.png not found" )
		endif()
#		# build a list of all the depends
#		list(APPEND PNG_RESULTS ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}${pix}.png)
	endif()
	list(APPEND IMG_RESULTS ${img1}.image1)
#	list(APPEND IMG_RESULTS ${img1}.png)
	if(WIN32)
		list(APPEND IMG_RESULTS ${img1}.xpm)
	endif()
endforeach()

add_custom_target (
	genbitmaps 
	DEPENDS ${IMG_RESULTS}
#	DEPENDS ${PNG_RESULTS}
)


#MESSAGE (STATUS "SVG Converter: ${SVG_CONVERTER}")
#MESSAGE (STATUS "Source Dir: ${CMAKE_CURRENT_SOURCE_DIR}")
#MESSAGE (STATUS "XPMs: ${IMG3S}")
#MESSAGE (STATUS "IMG_RESULTS: ${IMG_RESULTS}")
