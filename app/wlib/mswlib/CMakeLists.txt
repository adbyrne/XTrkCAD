#
# build the Win32 variant of the wlib
#


target_include_directories(xtrkcad-wlib 
	PRIVATE 
	${xtrkcad-lib_SOURCE_DIR}
)

if(XTRKCAD_USE_GETTEXT)
	if(WIN32)
		add_definitions(-DUSE_SIMPLE_GETTEXT )
	endif(WIN32)
endif(XTRKCAD_USE_GETTEXT)

target_sources(xtrkcad-wlib
	PRIVATE 
		backgnd.c
		getopt.c
		mswbox.c
		mswbutt.c
		mswbitmap.c
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
		utf8conv.c	
		mswint.h
		dynarr.h
	)

target_link_libraries(xtrkcad-wlib 
	PRIVATE
	Htmlhelp 
	msimg32 
	shlwapi
	FreeImage::FreeImage
)

install(FILES
	${FREEIMAGE_SHAREDLIB}
	DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
)

if(XTRKCAD_TESTING AND CMOCKA_FOUND)
	add_subdirectory( unittest )
endif()
