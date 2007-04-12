ADD_EXECUTABLE(cnvdsgn cnvdsgn.c utility.c)
TARGET_LINK_LIBRARIES(cnvdsgn m)
GET_TARGET_PROPERTY(cnvdsgn_EXE cnvdsgn LOCATION)

MACRO(GENERATE_LIN lin_name)
	ADD_CUSTOM_COMMAND(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${lin_name}.lin
		DEPENDS cnvdsgn ${CMAKE_CURRENT_SOURCE_DIR}/${lin_name}.src
		COMMAND ${cnvdsgn_EXE} < ${CMAKE_CURRENT_SOURCE_DIR}/${lin_name}.src > ${CMAKE_CURRENT_BINARY_DIR}/${lin_name}.lin
		)
ENDMACRO(GENERATE_LIN)

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

SET(LIN_SOURCES
	${CMAKE_CURRENT_BINARY_DIR}/to3way.lin
	${CMAKE_CURRENT_BINARY_DIR}/tocrv.lin
	${CMAKE_CURRENT_BINARY_DIR}/tocrvsct.lin
	${CMAKE_CURRENT_BINARY_DIR}/todcross.lin
	${CMAKE_CURRENT_BINARY_DIR}/todslip.lin
	${CMAKE_CURRENT_BINARY_DIR}/tolcross.lin
	${CMAKE_CURRENT_BINARY_DIR}/torcross.lin
	${CMAKE_CURRENT_BINARY_DIR}/toreg.lin
	${CMAKE_CURRENT_BINARY_DIR}/tosslip.lin
	${CMAKE_CURRENT_BINARY_DIR}/tostrsct.lin
	${CMAKE_CURRENT_BINARY_DIR}/towye.lin
	${CMAKE_CURRENT_BINARY_DIR}/toxing.lin
	)

SET(SOURCES
	${LIN_SOURCES}
	ccurve.c
	cdraw.c
	celev.c
	cgroup.c
	chndldto.c
	chotbar.c
	cjoin.c
	cmisc.c
	cmisc2.c
	cmodify.c
	cnote.c
	compound.c
	cparalle.c
	cprint.c
	cprofile.c
	cpull.c
	cruler.c
	cselect.c
	csnap.c
	csplit.c
	cstraigh.c
	cstruct.c
	ctext.c
	ctodesgn.c
	ctrain.c
	cturnout.c
	cturntbl.c
	cundo.c
	custom.c
	dbench.c
	dbitmap.c
	dcar.c
	dcmpnd.c
	dcustmgm.c
	dease.c
	denum.c
	dlayer.c
	doption.c
	dpricels.c
	dprmfile.c
	draw.c
	drawgeom.c
	elev.c
	fileio.c
	lprintf.c
	macro.c
	misc.c
	misc2.c
	param.c
	shrtpath.c
	tcurve.c
	tease.c
	track.c
	trkseg.c
	tstraigh.c
	utility.c
	)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
INCLUDE_DIRECTORIES(${help_BINARY_DIR})
INCLUDE_DIRECTORIES(${wlib_SOURCE_DIR}/include)

ADD_LIBRARY(xtrackcad-lib SHARED ${SOURCES})
TARGET_LINK_LIBRARIES(xtrackcad-lib xtrackcad-help)

ADD_EXECUTABLE(xtrackcad xtrackcad.c)
TARGET_LINK_LIBRARIES(xtrackcad xtrackcad-lib)
TARGET_LINK_LIBRARIES(xtrackcad xtrackcad-wlib)
TARGET_LINK_LIBRARIES(xtrackcad ${GTK_LIBRARIES})

ADD_EXECUTABLE(mkturnout
	${LIN_SOURCES}
	ctodesgn.c
	utility.c
	)
TARGET_LINK_LIBRARIES(mkturnout m)
SET_TARGET_PROPERTIES(mkturnout PROPERTIES COMPILE_FLAGS -DMKTURNOUT)

INSTALL(TARGETS xtrackcad mkturnout xtrackcad-lib
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	)

