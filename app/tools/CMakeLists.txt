
add_executable(listxtp listxtp.c)
set_target_properties(listxtp PROPERTIES FOLDER "Param Files")

if (WIN32)
	target_sources(listxtp PRIVATE dirent.c )
endif ()

add_subdirectory(halibut)
