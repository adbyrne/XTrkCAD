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
endif()    

target_include_directories(dynstring PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")