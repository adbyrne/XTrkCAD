project(xtrkcad-lib)

include( CheckSymbolExists )
#
# Create line symbols for turnouts from src definitions 
#
#	1. Build converter cnvdsgn
#   2. define target "lines" for all symols
#	3. create .lin-files using converter
#	4. add individual .lin-files as dependency to "lines"
#

# build GA or Beta executable
set( XTRKCAD_BIN "xtrkcad${XTRKCAD_BETA}" )

add_subdirectory(bitmaps)

add_executable(cnvdsgn cnvdsgn.c utility.c)

target_link_libraries(cnvdsgn xtrkcad-wlib)

if(NOT WIN32)
	target_link_libraries(cnvdsgn m)
endif()

add_custom_target(lines
	DEPENDS cnvdsgn
)

#
# macro to copy DLLs into build directory
# eases debugging on Windows
#
if(WIN32)
    macro(COPYDLLTOBUILD dllname)
        add_custom_command(
            TARGET xtrkcad POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            ${dllname}
            ${CMAKE_CURRENT_BINARY_DIR}
        )
    endmacro(COPYDLLTOBUILD)
endif()


#
# macro creates lin files and adds the result as dependency
#

macro(GENERATE_LIN lin_name)
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${lin_name}.lin
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${lin_name}.src
		COMMAND cnvdsgn < ${CMAKE_CURRENT_SOURCE_DIR}/${lin_name}.src > ${CMAKE_CURRENT_BINARY_DIR}/${lin_name}.lin
	)
	
	target_sources(lines 
		PRIVATE
			${CMAKE_CURRENT_BINARY_DIR}/${lin_name}.lin
	)
endmacro(GENERATE_LIN)

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

# put these targets into the folder LineSymbols
set_target_properties(lines cnvdsgn PROPERTIES FOLDER LineSymbols)

add_library(xtrkcad-lib "")

target_sources(xtrkcad-lib
	PRIVATE
    appdefaults.c
    archive.c
    archive.h
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
    problemrep.c
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
    toolbar.c
    turnout.c
    uthash.h
    utility.c
    utility.h
    validator.c
    validator.h
    include/dirent.h
    include/levenshtein.h
    include/paramfile.h
    include/paramfilelist.h
    include/partcatalog.h
    include/problemrep.h
    include/stringxtc.h
    include/utlist.h
)

# add UTF-8 conversion utilities on Windows

if(WIN32)
	target_sources(xtrkcad-lib
		PRIVATE
			utf8convert.c
			include/utf8convert.h			
	)
endif()

# This ensures that messages.h has been generated before we build xtrkcad-lib
add_dependencies(xtrkcad-lib 
    msgfiles
	lines
    genbitmaps
)


target_include_directories(xtrkcad-lib 
    PUBLIC
	${messagefile_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(xtrkcad-lib
    PRIVATE
    cJSON
	dynstring
	xtrkcad-cornu
	xtrkcad-wlib
)

if(UNIX AND NOT APPLE)
	target_link_libraries(xtrkcad-lib PRIVATE ${LIBZIP_LIBRARY} ${LIBZIP_LIBRARIES} ${ZLIB_LIBRARY})
else()
    target_link_libraries(xtrkcad-lib 
        PRIVATE 
        Libzip::Libzip 
        FreeImage::FreeImage 
        Zlib::Zlib)
endif()

#
# Start building the main program itself
#
add_executable(${XTRKCAD_BIN} WIN32 "" )

# if SVG export is built and source files and dependency on MiniXML

if(XTRKCAD_CREATE_SVG)
	target_sources(xtrkcad-lib
		PRIVATE
			svgformat.c
			svgoutput.c
	)

	if(WIN32)
        target_link_libraries(xtrkcad-lib 
            PRIVATE 
            MiniXML::mxml
        )
	endif()
	if(UNIX)
		if (APPLE OR NOT ${MINIXML_STATIC_LIBRARY})
            target_link_libraries(xtrkcad-lib 
                PRIVATE 
                MiniXML::mxml
            )       
        else()
			target_link_libraries(xtrkcad ${MINIXML_STATIC_LIBRARY})
			target_link_libraries(xtrkcad "pthread")
		endif()
	endif()
endif()

target_sources(${XTRKCAD_BIN}
	PRIVATE
		misc.c
        ${messagefile_BINARY_DIR}/bllnhlp.c
		xtrkcad.rc
)
add_dependencies(${XTRKCAD_BIN}
    balloonhelp
    msgfiles
    genbitmaps)
    
target_include_directories(${XTRKCAD_BIN}
    PRIVATE
	${messagefile_BINARY_DIR}    
)

target_link_libraries(${XTRKCAD_BIN}
    PRIVATE
		xtrkcad-wlib
		xtrkcad-lib
)

#
# Create mkturnout utility
#
add_executable(mkturnout
    ctodesgn.c
	utility.c
    trkendpt.c
)
set_target_properties(mkturnout PROPERTIES COMPILE_FLAGS -DMKTURNOUT)
set_target_properties(mkturnout PROPERTIES FOLDER "Param Files" )

target_include_directories(mkturnout
    PRIVATE
	${messagefile_BINARY_DIR}
)

target_link_libraries(mkturnout
    PRIVATE
    xtrkcad-wlib 
)

add_dependencies(mkturnout
    msgfiles
    lines
)

if(NOT WIN32)
	target_link_libraries(mkturnout 
        PRIVATE
        m
    )
	target_link_libraries(${XTRKCAD_BIN}
        PRIVATE
        m
    )

	# Link libintl for systems where it is a separate library
	find_library( INTL_LIBRARY intl )
	if(INTL_LIBRARY)
		target_link_libraries( mkturnout
            PRIVATE 
            ${INTL_LIBRARY}
        )
	endif()
else()
	# copy dlls into the build dir for easier debugging
    COPYDLLTOBUILD(${FREEIMAGE_SHAREDLIB})
#    COPYDLLTOBUILD(${LIBZIP_SHAREDLIB})
#    COPYDLLTOBUILD(${ZLIB_SHAREDLIB})
    set(ENV{path} "${CMAKE_CURRENT_BINARY_DIR};$ENV{PATH}")
	# add dll to install package
	install(
	    FILES ${LIBZIP_SHAREDLIB}
	    DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
	)
    install(
        FILES ${ZLIB_SHAREDLIB}
	    DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
	)

    # Copy program PDB file
    install(
        FILES $<TARGET_PDB_FILE:xtrkcad>
	    DESTINATION ${XTRKCAD_BIN_INSTALL_DIR} 
        OPTIONAL
	)
endif()

# for testing only, should be IF(APPLE) ...
if(APPLE)
	add_executable( helphelper helphelper.c )
	find_library(COREFOUNDATION_LIBRARY CoreFoundation)
	find_library(CARBON_LIBRARY Carbon)
	target_link_libraries(helphelper ${COREFOUNDATION_LIBRARY} ${CARBON_LIBRARY})
	install(
		TARGETS helphelper
		RUNTIME DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
		)
endif()

install(
	TARGETS ${XTRKCAD_BIN}
	RUNTIME DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
)

if(XTRKCAD_TESTING AND CMOCKA_FOUND)
	add_subdirectory( unittest )
endif()
