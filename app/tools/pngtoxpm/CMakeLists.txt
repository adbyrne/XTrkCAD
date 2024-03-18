# Build pngtoxpm

add_executable(pngtoxpm pngtoxpm.c)
target_link_libraries(pngtoxpm 
	PRIVATE
	FreeImage::FreeImage
)

if(WIN32)
# copy dlls into the build dir for easier debugging
	add_custom_command(
		TARGET pngtoxpm POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy ${FREEIMAGE_SHAREDLIB} ${CMAKE_CURRENT_BINARY_DIR}
	)
	set(ENV{path} "${CMAKE_CURRENT_BINARY_DIR};$ENV{PATH}")
endif()
