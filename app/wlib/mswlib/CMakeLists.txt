FILE(GLOB HEADERS *.h)

SET(SOURCES
#	checksum.c
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
	mswtext.c
	gwin32.c
	simple-gettext.c
	)

INCLUDE_DIRECTORIES(${XTrkCAD_BINARY_DIR})
# INCLUDE_DIRECTORIES(${XTRKCAD_BINARY_DIR})

IF(XTRKCAD_USE_GETTEXT)
	IF(WIN32)
		ADD_DEFINITIONS(-DUSE_SIMPLE_GETTEXT )
	ENDIF(WIN32)
ENDIF(XTRKCAD_USE_GETTEXT)

ADD_LIBRARY(xtrkcad-wlib ${HEADERS} ${SOURCES})

TARGET_LINK_LIBRARIES(xtrkcad-wlib Htmlhelp msimg32 shlwapi)

