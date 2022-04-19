
add_executable(listxtp listxtp.c)

if (WIN32)
	target_sources(listxtp PRIVATE dirent.c )
endif ()

add_subdirectory(halibut)
