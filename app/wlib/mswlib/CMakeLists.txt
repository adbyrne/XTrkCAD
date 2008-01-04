FILE(GLOB HEADERS *.h)

SET(SOURCES
#	checksum.c
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

ADD_LIBRARY(xtrkcad-wlib ${HEADERS} ${SOURCES})

TARGET_LINK_LIBRARIES(xtrkcad-wlib Htmlhelp)

