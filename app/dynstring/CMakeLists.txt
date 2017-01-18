# compile the dynstring library

set( SOURCES
	 dynstring.c )

add_library(dynstring
			${SOURCES})

if(XTRKCAD_TESTING)			
	add_subdirectory( unittest )
endif(XTRKCAD_TESTING)              