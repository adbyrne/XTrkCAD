project(xtrkcad-lib C)

include( CheckSymbolExists )
#
# Create line symbols for turnouts from src definitions
#
#	1. Build converter cnvdsgn
#	2. define target "lines" for all symbols
#	3. create .lin-files using converter
#	4. add individual .lin-files as dependency to "lines"
#

# XTRKCAD_BIN (the versioned executable name) is defined once in the
# top-level CMakeLists.txt so packaging and the executable target agree.

set( XTRKCAD_CORE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_subdirectory(bitmaps)

add_executable(cnvdsgn cnvdsgn.c utility.c)

target_include_directories(cnvdsgn
    PUBLIC
    ${XTRKCAD_CORE_INCLUDE_DIR}
)

target_link_libraries(cnvdsgn PRIVATE xtrkcad-wlib)

target_include_directories(cnvdsgn
    PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/include
)

if(NOT WIN32)
	target_link_libraries(cnvdsgn PRIVATE m)
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
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${lin_name}.src cnvdsgn
		COMMAND ${CMAKE_COMMAND}
			-DCNVDSGN=$<TARGET_FILE:cnvdsgn>
			-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/${lin_name}.src
			-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/${lin_name}.lin
			-P ${CMAKE_CURRENT_SOURCE_DIR}/run_cnvdsgn.cmake
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

# paths.c's HASH_FIND_STR/HASH_ADD_STR calls expand from the vendored
# uthash.h macro, which Clang's -Wimplicit-fallthrough misattributes to the
# call site (not a real switch fallthrough in this file) -- exempt this one
# file from the ratchet rather than the vendored uthash.h header itself,
# since it's the only owned file that hits this macro-attribution quirk.
# CMAKE_C_COMPILER_ID is "AppleClang" (not "Clang") for Xcode/macOS Clang --
# match both (caught by c-tests-macos CI failing without this).
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set_source_files_properties(paths.c PROPERTIES COMPILE_OPTIONS "-Wno-error=implicit-fallthrough")
endif()

target_sources(xtrkcad-lib
    PRIVATE
    # Sources
    appdefaults.c
    archive.c
    cbezier.c
    cblock.c
    ccornu.c
    ccurve.c
    ccontrol.c
    cdescribe.c
    cdraw.c
    celev.c
    cgroup.c
    chndldto.c
    cjoin.c
    cmodify.c
    cnote.c
    command.c
    compound.c
    cparalle.c
    cprint.c
    cprofile.c
    cpull.c
    cruler.c
    cselect.c
    csensor.c
    csnap.c
    csplit.c
    csignal.c
    cstraigh.c
    cstruct.c
    cswitchmotor.c
    ctext.c
    ctodesgn.c
    ctrain.c
    cturnout.c
    cturntbl.c
    cundo.c
    undostream.c
    custom.c
    dbench.c
    dbitmap.c
    cars/carcommit.c
    cars/caritem.c
    cars/carpart.c
    cars/carproto.c
    cars/careditdlg.c
    cars/careditlogic.c
    cars/carinvdlg.c
    cars/dcar.c
    cars/tabstring.c
    cars/transform_pts.c
    cars/listelem.c
    dcmdoptions.c
    dcmpnd.c
    dcustmgm.c
    dcontmgm.c
    ddrawprim.c
    dease.c
    denum.c
    directory.c
    dlayer.c
    dlayergroup.c
    dlayergroupui.c
    dselectlayers.c
    doption.c
    dpreferences.c
    dprmfile.c
    draw.c
    drawgeom.c
    drawruler.c
    dxfformat.c
    dxfimport.c
    dxfoutput.c
    elev.c
    elevjoin.c
    file2uri.c
    fileio.c
    filenoteui.c
    getargs.c
    icons.c
    i18n.c
    layout.c
    levenshtein.c
    linknoteui.c
    locale.c
    lprintf.c
    macro.c
    manifest.c
    mapwindow.c
    menu.c
    misc.c
    mymalloc.c
    # param.c
    paramfile.c
    paramfilelist.c
    paramfilesearch_ui.c
    partcatalog.c
    paths.c
    problemrep.c
    problemrepui.c
    reports.c
    reportsformat.c
    scale.c
    shortentext.c
    shrtpath.c
    smalldlg.c
    stringxtc.c
    tbezier.c
    tcornu.c
    tcurve.c
    tease.c
    textnoteui.c
    track.c
    trkendpt.c
    trknote.c
    trkseg.c
    tstraigh.c
    turnout.c
    utility.c
    validator.c

    # Headers
    archive.h
    cbezier.h
    ccornu.h
    ccurve.h
    cjoin.h
    command.h
    common-ui.h
    compound.h
    cselect.h
    cstraigh.h
    ctrain.h
    cundo.h
    undostream.h
    custom.h
    directory.h
    dlayer.h
    include/dlayergroup.h
    include/dlayergroupui.h
    include/dselectlayers.h
    draw.h
    drawgeom.h
    dxfformat.h
    elevjoin.h
    file2uri.h
    fileio.h
    layout.h
    manifest.h
    mapwindow.h
    menu.h
    misc.h
    note.h
    paths.h
    scale.h
    shortentext.h
    shrtpath.h
    smalldlg.h
    tbezier.h
    tcornu.h
    track.h
    trackx.h
    trkendpt.h
    trkendptx.h
    trkseg.h
    uthash.h
    utility.h
    validator.h
    include/levenshtein.h
    include/paramfile.h
    include/paramfilelist.h
    include/partcatalog.h
    include/problemrep.h
    include/reports.h
    include/stringxtc.h
    include/utlist.h
)

add_subdirectory(form)
add_subdirectory(appmainwindow)
add_subdirectory(parts)

# add UTF-8 conversion utilities on Windows

if(WIN32)
    target_sources(xtrkcad-lib
        PRIVATE
        utf8convert.c
        winstubs.c
        include/utf8convert.h
        include/win32/dirent.h
    )
endif()

# This ensures that messages.h has been generated before we build xtrkcad-lib
add_dependencies(xtrkcad-lib
    msgfiles
 lines
)
if(TARGET genbitmaps)
    add_dependencies(xtrkcad-lib genbitmaps)
endif()

target_include_directories(xtrkcad-lib
    PUBLIC
 ${messagefile_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${XTRKCAD_CORE_INCLUDE_DIR}
)

target_link_libraries(xtrkcad-lib
    PRIVATE
    cJSON
 dynstring
 xtrkcad-cornu
 xtrkcad-wlib
    parts
)

target_link_libraries(xtrkcad-lib
    PRIVATE
    Libzip::Libzip
    ZLIB::ZLIB)

if(MSVC)
    target_link_libraries(xtrkcad-lib
        PRIVATE
        FreeImage::FreeImage)
endif()
#
# Start building the main program itself
#
add_executable(${XTRKCAD_BIN} WIN32 "" )

# If SVG export is enabled, add its source files and the MiniXML dependency.

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
			target_link_libraries(${XTRKCAD_BIN} PRIVATE ${MINIXML_STATIC_LIBRARY} pthread)
        endif()
    endif()
endif()

target_sources(${XTRKCAD_BIN}
    PRIVATE
    misc.c
    ${messagefile_BINARY_DIR}/tooltiptext.c
    xtrkcad.rc
)
add_dependencies(${XTRKCAD_BIN}
    tooltips
    msgfiles)
if(TARGET genbitmaps)
    add_dependencies(${XTRKCAD_BIN} genbitmaps)
endif()

target_include_directories(${XTRKCAD_BIN}
    PRIVATE
 ${messagefile_BINARY_DIR}
)

target_link_libraries(${XTRKCAD_BIN}
    PRIVATE
  xtrkcad-wlib
  xtrkcad-lib
)



if(NOT WIN32)
    target_link_libraries(${XTRKCAD_BIN}
        PRIVATE
        m
    )
else()
    set(ENV{path} "${CMAKE_CURRENT_BINARY_DIR};$ENV{PATH}")

    if(NOT MINGW)
        # MSVC build: install explicit DLLs (libzip, zlib)
        install(FILES ${LIBZIP_SHAREDLIB} DESTINATION ${XTRKCAD_BIN_INSTALL_DIR})
        install(FILES ${ZLIB_SHAREDLIB} DESTINATION ${XTRKCAD_BIN_INSTALL_DIR})
        if(MSVC)
            install(
                FILES $<TARGET_PDB_FILE:xtrkcad>
                DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
                OPTIONAL
            )
        endif()
    endif()
    # MinGW/MSYS2: GTK3 runtime DLLs are collected by the release workflow
    # (ntldd-based script) before packaging. NSIS DLL bundling via CMake
    # install(RUNTIME_DEPENDENCY_SET) is deferred — the keyword is not
    # recognised by the MSYS2 CMake 4.x build.
endif()

# for testing only, should be IF(APPLE) ...
if(APPLE)
    add_executable( helphelper helphelper.c )
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    find_library(CARBON_LIBRARY Carbon)
	target_link_libraries(helphelper PRIVATE ${COREFOUNDATION_LIBRARY} ${CARBON_LIBRARY})
    install(
  TARGETS helphelper
  RUNTIME DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
  )
endif()

install(
    TARGETS ${XTRKCAD_BIN}
    RUNTIME DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
)

if (XTRKCAD_BUILD_FLATPAK)
    install(
        FILES ${CMAKE_SOURCE_DIR}/distribution/flatpak/common_source.sh
        DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
    )

    install(
        FILES ${CMAKE_SOURCE_DIR}/distribution/flatpak/run-xtrkcad.sh
        DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
    )
endif()

if(XTRKCAD_TESTING AND CMOCKA_FOUND)
    add_subdirectory( unittest )
endif()

if(XTRKCAD_REGRESSION_TESTING)
    include( "${CMAKE_CURRENT_LIST_DIR}/RegressionTests.cmake" )
endif()

if(XTRKCAD_FUZZ)
    add_subdirectory( fuzz )
endif()
