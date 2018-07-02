find_package(FreeImage REQUIRED)

FILE(GLOB HEADERS *.h)

SET(SOURCES
#	checksum.c
	backgnd.c
	getopt.c
	mswbox.c
	mswbutt.c
	mswbitmap.c
	mswchksm.c
	mswchoic.c
	mswcolor.c
	mswdraw.c
	mswedit.c
	mswlines.c
	mswlist.c
	mswmenu.c
	mswmisc.c
	mswmsg.c
	mswpref.c
	mswprint.c
	mswsplash.c
	mswstatus.c
	mswtext.c
	gwin32.c
	simple-gettext.c
	)

include_directories(${FREEIMAGE_INCLUDE_PATH})
INCLUDE_DIRECTORIES(${XTrkCAD_BINARY_DIR})

IF(XTRKCAD_USE_GETTEXT)
	IF(WIN32)
		ADD_DEFINITIONS(-DUSE_SIMPLE_GETTEXT )
	ENDIF(WIN32)
ENDIF(XTRKCAD_USE_GETTEXT)

ADD_LIBRARY(xtrkcad-wlib ${HEADERS} ${SOURCES})

TARGET_LINK_LIBRARIES(xtrkcad-wlib Htmlhelp msimg32 shlwapi)
target_link_libraries(xtrkcad-wlib ${FREEIMAGE_LIBRARY})

install(FILES
	${FREEIMAGE_SHAREDLIB}
	DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
	)
