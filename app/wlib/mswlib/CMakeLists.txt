FILE(GLOB HEADERS *.h)

SET(SOURCES
#	checksum.c
	getopt.c
	mswbox.c
	mswbutt.c
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
	)

INCLUDE_DIRECTORIES(${XTrkCAD_BINARY_DIR})
# INCLUDE_DIRECTORIES(${XTRKCAD_BINARY_DIR})

IF(XTRKCAD_USE_GETTEXT)
	#
	# Find the GnuWin32 installation directory, the gettext include should be located in subdir include
	#
	IF(WIN32)
		GET_FILENAME_COMPONENT( GETTEXTPATH  
				[HKEY_LOCAL_MACHINE\\SOFTWARE\\GnuWin32;InstallPath]  
				ABSOLUTE)
		INCLUDE_DIRECTORIES(${GETTEXTPATH}/include)
		LINK_DIRECTORIES(${GETTEXTPATH}/lib)
	ENDIF(WIN32)
ENDIF(XTRKCAD_USE_GETTEXT)

ADD_LIBRARY(xtrkcad-wlib ${HEADERS} ${SOURCES})

TARGET_LINK_LIBRARIES(xtrkcad-wlib Htmlhelp msimg32)

