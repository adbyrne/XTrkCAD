# compile the dynstring library

add_library(dynstring
			"")

target_sources(dynstring
	PRIVATE
		dynstring.c
	PUBLIC 
		dynstring.h
)			

if(XTRKCAD_TESTING)			
	add_subdirectory(unittest)
endif(XTRKCAD_TESTING)              