
FILE(GLOB HEADERS *.h)

SET(SOURCES
	bezctx.c
	bezctx_xtrkcad.c
	spiro.c
	)


INCLUDE_DIRECTORIES(${XTrkCAD_BINARY_DIR})

ADD_LIBRARY(xtrkcad-cornu ${HEADERS} ${SOURCES})



