include( CheckSymbolExists )

ADD_EXECUTABLE(cnvdsgn cnvdsgn.c utility.c)

IF(NOT WIN32)
	TARGET_LINK_LIBRARIES(cnvdsgn m)
ENDIF(NOT WIN32)

MACRO(GENERATE_LIN lin_name)
	ADD_CUSTOM_COMMAND(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${lin_name}.lin
		DEPENDS cnvdsgn ${CMAKE_CURRENT_SOURCE_DIR}/${lin_name}.src
		COMMAND cnvdsgn < ${CMAKE_CURRENT_SOURCE_DIR}/${lin_name}.src > ${CMAKE_CURRENT_BINARY_DIR}/${lin_name}.lin
		)
ENDMACRO(GENERATE_LIN)

GENERATE_LIN(to3way)
GENERATE_LIN(tocrv)
GENERATE_LIN(tocrvsct)
GENERATE_LIN(todcross)
GENERATE_LIN(todslip)
GENERATE_LIN(tolcross)
GENERATE_LIN(torcross)
GENERATE_LIN(toreg)
GENERATE_LIN(tosslip)
GENERATE_LIN(tostrsct)
GENERATE_LIN(towye)
GENERATE_LIN(toxing)
GENERATE_LIN(tocornu)
GENERATE_LIN(tocornuwye)
GENERATE_LIN(tocornu3way)

SET(LIN_SOURCES
	${CMAKE_CURRENT_BINARY_DIR}/to3way.lin
	${CMAKE_CURRENT_BINARY_DIR}/tocrv.lin
	${CMAKE_CURRENT_BINARY_DIR}/tocrvsct.lin
	${CMAKE_CURRENT_BINARY_DIR}/todcross.lin
	${CMAKE_CURRENT_BINARY_DIR}/todslip.lin
	${CMAKE_CURRENT_BINARY_DIR}/tolcross.lin
	${CMAKE_CURRENT_BINARY_DIR}/torcross.lin
	${CMAKE_CURRENT_BINARY_DIR}/toreg.lin
	${CMAKE_CURRENT_BINARY_DIR}/tosslip.lin
	${CMAKE_CURRENT_BINARY_DIR}/tostrsct.lin
	${CMAKE_CURRENT_BINARY_DIR}/towye.lin
	${CMAKE_CURRENT_BINARY_DIR}/toxing.lin
	${CMAKE_CURRENT_BINARY_DIR}/tocornu.lin
	${CMAKE_CURRENT_BINARY_DIR}/tocornuwye.lin
	${CMAKE_CURRENT_BINARY_DIR}/tocornu3way.lin
	)

ADD_CUSTOM_COMMAND(
       OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/bllnhlp.c
       DEPENDS genhelp ${help_SOURCE_DIR}/genhelp.json
       COMMAND genhelp ${GENHELP_OPTS} ${help_SOURCE_DIR}/genhelp.json ${CMAKE_CURRENT_BINARY_DIR}/bllnhlp.c
       )

ADD_EXECUTABLE(xtrkcad WIN32
 	misc.c
 	xtrkcad.rc
)

SET(SOURCES
	${LIN_SOURCES}
	appdefaults.c
	archive.c
	archive.h
	bllnhlp.c
	cbezier.c
	cbezier.h
	cblock.c
	ccornu.c
	ccornu.h
	ccurve.c
	ccurve.h
	ccontrol.c
	cdraw.c
	celev.c
	cgroup.c
	chndldto.c
	chotbar.c
	cjoin.c
	cjoin.h
	cJSON.c
	cJSON.h
	cmisc.c
	cmodify.c
	cnote.c
	command.c
	command.h
	common.h
	common-ui.h
	compound.c
	compound.h
	cparalle.c
	cprint.c
	cprofile.c
	cpull.c
	cruler.c
	cselect.c
	cselect.h
	csensor.c
	csnap.c
	csplit.c
	csignal.c
	cstraigh.c
	cstraigh.h
	cstruct.c
	cswitchmotor.c
	ctext.c
	ctodesgn.c
	ctrain.c
	ctrain.h
	cturnout.c
	cturntbl.c
	cundo.c
	cundo.h
	custom.c
	custom.h
	dbench.c
	dbitmap.c
	dcar.c
	dcmpnd.c
	dcustmgm.c
	dcontmgm.c
	dease.c
	denum.c
	directory.c
	directory.h
	dlayer.c
	dlayer.h
	doption.c
	dpricels.c
	dprmfile.c
	draw.c
	draw.h
	drawgeom.c
	drawgeom.h
	dxfformat.c
	dxfformat.h
	dxfoutput.c
	elev.c
	file2uri.c
	file2uri.h
	fileio.c
	fileio.h
	filenoteui.c
	i18n.c
	layout.c
	layout.h
	levenshtein.c
	linknoteui.c
	lprintf.c
	macro.c
	manifest.c
	manifest.h
	menu.h
	menu.c
	misc.c
	misc.h
	note.h
	param.c
	param.h
	paramfile.c
	paramfilelist.c
	paramfilesearch_ui.c
	partcatalog.c
	paths.c
	paths.h
	scale.c
	scale.h
	shortentext.c
	shortentext.h
	shrtpath.c
	shrtpath.h
	smalldlg.c
	smalldlg.h
	stringxtc.c
	tbezier.c
	tbezier.h
	tcornu.c
	tcornu.h
	tcurve.c
	tease.c
	textnoteui.c
	track.c
	track.h
	trackx.h
	trkendpt.c
	trkendpt.h
	trkendptx.h
	trknote.c
	trkseg.h
	trkseg.c
	tstraigh.c
	turnout.c
	uthash.h
	utility.c
	utility.h
	validator.c
	validator.h
	)

# add UTF-8 conversion utilities on Windows
if(WIN32)
	set( SOURCES
		${SOURCES}
		utf8convert.c
		include/utf8convert.h
	)
endif(WIN32)

set (SOURCES
	${SOURCES}
	include/dirent.h
	include/levenshtein.h
	include/paramfile.h
	include/paramfilelist.h
	include/partcatalog.h
	include/stringxtc.h
	include/utlist.h
)

if(XTRKCAD_CREATE_SVG)
	set (SOURCES
	  ${SOURCES}
		svgformat.c
		svgoutput.c
	)
	include_directories(${MINIXML_INCLUDE_PATH})
	if(WIN32)

	    add_custom_command(
		TARGET xtrkcad POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		${MINIXML_SHAREDLIB}
		${CMAKE_CURRENT_BINARY_DIR}
	    )

            # add dll to install package
	    install(FILES ${MINIXML_SHAREDLIB}
        	DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
	    )
	endif(WIN32)

endif(XTRKCAD_CREATE_SVG)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(${XTrkCAD_BINARY_DIR})
INCLUDE_DIRECTORIES(${help_BINARY_DIR})
INCLUDE_DIRECTORIES(${wlib_SOURCE_DIR}/include)
include_directories(${FREEIMAGE_INCLUDE_PATH})
INCLUDE_DIRECTORIES(${LIBZIP_INCLUDE_DIR_ZIP})
INCLUDE_DIRECTORIES(${CJSON_INCLUDE})

LINK_DIRECTORIES(${GTK_LIBRARY_DIRS})
LINK_DIRECTORIES(${GTK_WEBKIT_LIBRARY_DIRS})
LINK_DIRECTORIES(${LIBZIP_LIBZIP_LIBRARY})

ADD_LIBRARY(xtrkcad-lib ${SOURCES})

# This ensures that messages.h has been generated before we build xtrkcad-lib
ADD_DEPENDENCIES(xtrkcad-lib Help)

TARGET_LINK_LIBRARIES(xtrkcad xtrkcad-lib)
TARGET_LINK_LIBRARIES(xtrkcad xtrkcad-wlib)
TARGET_LINK_LIBRARIES(xtrkcad xtrkcad-cornu)
TARGET_LINK_LIBRARIES(xtrkcad dynstring)
if(XTRKCAD_CREATE_SVG)
IF(UNIX)
	if (NOT APPLE)
		target_link_libraries(xtrkcad ${MINIXML_STATIC_LIBRARY})
		target_link_libraries(xtrkcad "pthread")
	else(NOT APPLE)
		target_link_libraries(xtrkcad ${MINIXML_LIBRARY})
	endif(NOT APPLE)
ELSE(UNIX)
	target_link_libraries(xtrkcad ${MINIXML_STATIC_LIBRARY})
ENDIF(UNIX)
endif(XTRKCAD_CREATE_SVG)

IF(UNIX AND NOT APPLE)
target_link_libraries(xtrkcad ${LIBZIP_LIBRARY} ${LIBZIP_LIBRARIES} ${ZLIB_LIBRARY})
ELSE(UNIX AND NOT APPLE)
target_link_libraries(xtrkcad ${LIBZIP_LIBRARY} ${LIBZIP_LIBRARIES})
ENDIF(UNIX AND NOT APPLE)

ADD_EXECUTABLE(mkturnout
	${LIN_SOURCES}
	ctodesgn.c
	utility.c
	trkendpt.c

	)
SET_TARGET_PROPERTIES(mkturnout PROPERTIES COMPILE_FLAGS -DMKTURNOUT)

TARGET_LINK_LIBRARIES(mkturnout xtrkcad-cornu)

IF(NOT WIN32)
	TARGET_LINK_LIBRARIES(mkturnout m)
	TARGET_LINK_LIBRARIES(xtrkcad m)

	# Link libintl for systems where it is a separate library
	find_library( INTL_LIBRARY intl )
	if(INTL_LIBRARY)
		target_link_libraries( mkturnout ${INTL_LIBRARY} )
	endif(INTL_LIBRARY)
ELSE(NOT WIN32)
	TARGET_LINK_LIBRARIES(mkturnout xtrkcad-wlib)

	# copy dlls into the build dir for easier debugging
	add_custom_command(
		TARGET xtrkcad POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		${FREEIMAGE_SHAREDLIB}
		${CMAKE_CURRENT_BINARY_DIR}
	)

	add_custom_command(
		TARGET xtrkcad POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		${LIBZIP_SHAREDLIB}
		${CMAKE_CURRENT_BINARY_DIR}
	)

	add_custom_command(
		TARGET xtrkcad POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		${ZLIB_SHAREDLIB}
		${CMAKE_CURRENT_BINARY_DIR}
	)

	# add dll to install package
	INSTALL(
	    FILES
	    ${LIBZIP_SHAREDLIB}
	    DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
	)
	    INSTALL(FILES
	    ${ZLIB_SHAREDLIB}
	    DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
	)

        # Copy program PDB file
        INSTALL(
            FILES $<TARGET_PDB_FILE:xtrkcad>
	    DESTINATION ${XTRKCAD_BIN_INSTALL_DIR} OPTIONAL
	)
ENDIF(NOT WIN32)

# for testing only, should be IF(APPLE) ...
IF(APPLE)
	ADD_EXECUTABLE( helphelper helphelper.c )
	FIND_LIBRARY(COREFOUNDATION_LIBRARY CoreFoundation)
	FIND_LIBRARY(CARBON_LIBRARY Carbon)
	TARGET_lINK_LIBRARIES(helphelper ${COREFOUNDATION_LIBRARY} ${CARBON_LIBRARY})
	INSTALL(
		TARGETS helphelper
		RUNTIME DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
		)
ENDIF(APPLE)

INSTALL(
	TARGETS xtrkcad
	RUNTIME DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
	)

if(XTRKCAD_TESTING AND CMOCKA_FOUND)
	add_subdirectory( unittest )
endif()
