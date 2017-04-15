
# $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/lib/CMakeLists.txt,v 1.6 2008-03-27 18:17:16 m_fischer Exp $
#

PROJECT(lib)

ADD_SUBDIRECTORY(demos)
ADD_SUBDIRECTORY(examples)
ADD_SUBDIRECTORY(params)

INSTALL(FILES 
	COPYING
	Readme.txt
	logo.bmp
	xtrkcad.xtq
	DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}
	)

IF(UNIX AND NOT APPLE)
	INSTALL(FILES
		xtrkcad.desktop
		DESTINATION "/usr/share/applications"
		)
	INSTALL(FILES
		xtrkcad.png
		DESTINATION "/usr/share/pixmaps"
		)
ENDIF(UNIX AND NOT APPLE)