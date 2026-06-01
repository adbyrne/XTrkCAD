# Generate bitmaps from SVG files and other places
#
# FLATPAK builds use pregenerated bitmap files.  See below;
#
# We generate different types of bitmap files depending of the platform
# Both use the common suffix .image1 (this saves lots of ugly preprocesor code)
# Unix - ascii-ized version of the .png files (using gdk_pixbuf_csource)
# Window - .xpm files (using pngtoxpm)
# 
# Toolbar button icons come in 3 sizes.  Separate .image1 files are generated for each size
# A small #include file (.image3) is generated for each icon, Which #include's the 3 .image1 files
# and an array definition to select the bitmap for the size
# Toolbar icons are defined as .svg file which are converated by Inkscape into .png files.
# Some of the generated bitmap are ugly and handcrafted .png files are used instead
# This live in ${CMAKE_CURRENT_SOURCE}/png
#
# Another group of icons are used as dialog decorations.
# These .png files live in ${CMAKE_CURRENT_SOURCE_DIR}

PROJECT(genbitmaps C)

set (IMG3S
	background benchwork bezier-line bezier-track block bottom box
	bridge building car-inventory change-grid circle-center
	circle-filled-center circle-filled-tangent circle-line-center
	circle-line-tangent circle-line circle-tangent circle clone
	connect control convert-from convert-to copy cornu curved-chord
	curved-end curved-line-chord curved-line-end curved-line-middle
	curved-line-tangent curved-middle curved-tangent cut delete
	describe description dimension doc-export-bmap doc-export-dxf
	doc-export-svg doc-export doc-import-mod doc-import 
	doc-new doc-open doc-print doc-recent doc-revert doc-save-as 
	doc-save doc-setup down ease-broad ease-cornu ease-gt-broad 
	ease-gt-sharp ease-lt-broad ease-lt-sharp ease-none ease-normal 
	ease-sharp elevation exit extend filled-box filled-polygon go 
	helix info join-line join layers magnet manage map move
	new-car ok pan-zoom parallel-line parallel parameter paste
	polygon polyline profile protractor redo reflect roadbed
	rotate ruler select sensor signal snap-curs snap-grid
	split-draw split sticky-doc sticky-link sticky-note stop
	straight-line straight switch-motor table-edge text ties top
	train trim tunnel turnout-design turnout turntable undo
	zoom-choose zoom-extent zoom-in zoom-out
	)
set( PIXS 16 24 32 )

set (IMG1S
	ballgreen ballred bluedot bo_edge bo_flat bo_lld bo_lli bo_llu bo_ll bo_lrd bo_lri
	bo_lru bo_lr bo_ti bo_tl bo_tr bo_t carpart carproto funnelclear funnel greendot
	greenstar greydot greystar loosen note partlist pause reddot redstar struct xtc
	yellowdot yellowstar zero
)

# If ../../../bitmaps exists it is because we are building for FlatPak which can't
# build bitmaps because we don't have inkscape
# Instead we copy the bitmaps from bia regular build and include them at the top
# level of the source tree in the tarball we create for the FlatPak build
# See distribution/flatpak/make-source-archive
# So if bitmaps/ exists then we are doing a FlatPak build
if ( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../bitmaps/" )
	# FLATPAK
	MESSAGE( STATUS "FlatPak build" )
	foreach( img1 ${IMG1S} )
		list(APPEND IMG_RESULTS ${img1}.image1)
	endforeach()
	foreach( img3 ${IMG3S} )
		foreach( pix 16 24 32 )
			list(APPEND IMG_RESULTS ${img3}${pix}.image1)
		endforeach()
		list(APPEND IMG_RESULTS ${img3}.image3)
	endforeach()

	foreach ( img ${IMG_RESULTS} )
		add_custom_command(
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../../bitmaps/${img}
			OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${img}
			COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../../../bitmaps/${img} ${CMAKE_CURRENT_BINARY_DIR}/${img}
		)
	endforeach()
	MESSAGE( STATUS ${IMG_RESULTS} )

	add_custom_target (
		genbitmaps 
		DEPENDS ${IMG_RESULTS}
	)
	return()
endif()

# Find the conversion tool 
find_package(Inkscape)

if( NOT Inkscape_FOUND )
	message( STATUS "Inkscape not found." )
	return()
else()
	message(STATUS "Inkscape is: ${Inkscape_EXECUTABLE}")
	message(STATUS "Export option is: ${Inkscape_EXPORT}")
endif()


foreach(img3 ${IMG3S})
	STRING(MAKE_C_IDENTIFIER ${img3} NAME )
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
		if (XTRKCAD_BUILD_FLATPAK)
		add_custom_command(
			OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}${pix}.png
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/svg/${img3}.svg
			COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/png/
			COMMAND ${Inkscape_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/svg/${img3}.svg -h ${pix} -o ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}${pix}.png
		)
		elseif(Inkscape_IS_RSVG)
		add_custom_command(
			OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}${pix}.png
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/svg/${img3}.svg
			COMMAND ${CMAKE_COMMAND}
				-DEXE=${Inkscape_EXECUTABLE}
				-DWIDTH=${pix}
				-DHEIGHT=${pix}
				"-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/svg/${img3}.svg"
				"-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/png/${img3}${pix}.png"
				-P ${CMAKE_CURRENT_SOURCE_DIR}/run_rsvg.cmake
		)
		else()
		add_custom_command(
			OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}${pix}.png
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/svg/${img3}.svg
			COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/png/
			COMMAND ${Inkscape_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/svg/${img3}.svg -h ${pix} ${Inkscape_EXPORT}${CMAKE_CURRENT_BINARY_DIR}/png/${img3}${pix}.png
		)
		endif()
		endif()
		if(UNIX)
			add_custom_command(
				DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}${pix}.png
				OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${img3}${pix}.image1
				COMMAND gdk-pixbuf-csource --stream --name ${NAME}${pix}_image1 ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}${pix}.png >  ${CMAKE_CURRENT_BINARY_DIR}/${img3}${pix}.image1
			)
		else()
			add_custom_command(
				DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/png/${img3}${pix}.png pngtoxpm
				OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${img3}${pix}.image1
				COMMAND ${CMAKE_COMMAND}
					-DEXE=$<TARGET_FILE:pngtoxpm>
					-DNAME=${img3}${pix}_image1
					"-DINPUT=${CMAKE_CURRENT_BINARY_DIR}/png/${img3}${pix}.png"
					"-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/${img3}${pix}.image1"
					-P ${CMAKE_CURRENT_SOURCE_DIR}/run_pngtoxpm.cmake
			)
		endif()
	endforeach()
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${img3}.image3
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${img3}16.image1
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${img3}24.image1
		DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${img3}32.image1
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/mkimage3.cmake ${img3}
	)
	list(APPEND IMG_RESULTS ${img3}.image3)
endforeach()

foreach(img1 ${IMG1S})
	STRING(MAKE_C_IDENTIFIER ${img1} NAME )
	if(UNIX)
		add_custom_command(
			OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${img1}.image1
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${img1}.png
			COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/
			COMMAND gdk-pixbuf-csource --stream --name ${NAME}_image1 ${CMAKE_CURRENT_SOURCE_DIR}/${img1}.png >  ${CMAKE_CURRENT_BINARY_DIR}/${img1}.image1
		)
	else()
		add_custom_command(
			OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${img1}.image1
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${img1}.png pngtoxpm
			COMMAND ${CMAKE_COMMAND}
				-DEXE=$<TARGET_FILE:pngtoxpm>
				-DNAME=${img1}_image1
				"-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/${img1}.png"
				"-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/${img1}.image1"
				-P ${CMAKE_CURRENT_SOURCE_DIR}/run_pngtoxpm.cmake
		)
	endif()
	list(APPEND IMG_RESULTS ${img1}.image1)
	list(APPEND IMG_RESULTS ${img1}.png)
endforeach()

add_custom_target (
	genbitmaps 
	DEPENDS ${IMG_RESULTS}
)



#MESSAGE (STATUS "SVG Converter: ${SVG_CONVERTER}")
#MESSAGE (STATUS "Source Dir: ${CMAKE_CURRENT_SOURCE_DIR}")
#MESSAGE (STATUS "XPMs: ${IMG3S}")
#MESSAGE (STATUS "IMG_RESULTS: ${IMG_RESULTS}")
