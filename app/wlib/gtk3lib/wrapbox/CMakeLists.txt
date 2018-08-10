PROJECT(wrapbox)
# Setup GTK UI library...
file(GLOB headers *.h)

set(SOURCES
	eggwrapbox.c	
	)
	
SET(HEADERS 
	eggwrapbox.h
	eggwrapbox-enums.h
	)

	
INCLUDE_DIRECTORIES(${XTrkCAD_BINARY_DIR})
INCLUDE_DIRECTORIES(${XTrkCAD_SOURCE_DIR})
INCLUDE_DIRECTORIES(${wlib_SOURCE_DIR}/include)
INCLUDE_DIRECTORIES(${help_BINARY_DIR})

LINK_DIRECTORIES(${GTK_LIBRARY_DIRS})

ADD_LIBRARY(wrapbox ${HEADERS} ${SOURCES})

target_link_libraries(wrapbox PRIVATE ${GTK_LIBRARIES})

#INSTALL(
#		TARGETS wrapbox
#		DESTINATION ${XTRKCAD_BIN_INSTALL_DIR}
#		)