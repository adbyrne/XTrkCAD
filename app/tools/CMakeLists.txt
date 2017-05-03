
set ( sources listxtp.c )

if (WIN32)
	set ( sources
		  ${sources}	
		  dirent.c)
		  
	include_directories( before ${CMAKE_CURRENT_SOURCE_DIR})	  
endif ()
				 
add_executable( listxtp ${sources})

add_subdirectory(halibut)

