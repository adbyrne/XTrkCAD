# Build pngtoxpm

find_package(FreeImage REQUIRED)

include_directories(${FREEIMAGE_INCLUDE_PATH})

SET(SOURCES
	pngtoxpm.c
	)

ADD_EXECUTABLE(pngtoxpm ${SOURCES})
TARGET_LINK_LIBRARIES(pngtoxpm ${FREEIMAGE_LIBRARY})

# copy dlls into the build dir for easier debugging
add_custom_command(
	TARGET pngtoxpm POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy
	${FREEIMAGE_SHAREDLIB}
	${CMAKE_CURRENT_BINARY_DIR}
)
