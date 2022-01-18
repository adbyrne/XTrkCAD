find_package(FreeImage REQUIRED)

SET(SOURCES
	pngtoxpm.c
	)

ADD_EXECUTABLE(pngtoxpm ${SOURCES})
TARGET_LINK_LIBRARIES(pngtoxpm ${FREEIMAGE_LIBRARY})

