# Build pngtoxpm

find_package(FreeImage REQUIRED)

include_directories(${FREEIMAGE_INCLUDE_PATH})

SET(SOURCES
	pngtoxpm.c
	)

ADD_EXECUTABLE(pngtoxpm ${SOURCES})
TARGET_LINK_LIBRARIES(pngtoxpm ${FREEIMAGE_LIBRARY})

