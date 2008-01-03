PROJECT(lib)

ADD_SUBDIRECTORY(demos)
ADD_SUBDIRECTORY(examples)
ADD_SUBDIRECTORY(params)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/xtrkcad.ini ${CMAKE_CURRENT_BINARY_DIR}/xtrkcad.ini)

INSTALL(FILES 
	COPYING
	aareadme.txt
	logo.bmp
	xtrkcad.bug
	xtrkcad.enh
	xtrkcad.fix
	${CMAKE_CURRENT_BINARY_DIR}/xtrkcad.ini
	xtrkcad.spec.head
	xtrkcad.xtq
	DESTINATION share
	)
