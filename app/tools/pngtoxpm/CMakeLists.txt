# Build pngtoxpm

add_executable(pngtoxpm pngtoxpm.c)
target_link_libraries(pngtoxpm 
	PRIVATE
	FreeImage::FreeImage
)

if(WIN32)
	add_custom_command(
		TARGET pngtoxpm POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy ${FREEIMAGE_SHAREDLIB} $<TARGET_FILE_DIR:pngtoxpm>
	)
endif()
