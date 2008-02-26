PROJECT(lib)

ADD_SUBDIRECTORY(demos)
ADD_SUBDIRECTORY(examples)
ADD_SUBDIRECTORY(params)

INSTALL(FILES 
	COPYING
	aareadme.txt
	logo.bmp
	xtrkcad.bug
	xtrkcad.enh
	xtrkcad.fix
	xtrkcad.spec.head
	xtrkcad.xtq
	DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}
	)
