project(wlib)

add_library( xtrkcad-wlib
			"")		

add_subdirectory(include)

# add i18n.h to include path
target_include_directories(xtrkcad-wlib 
	PRIVATE 
	${xtrkcad-lib_SOURCE_DIR}
)

if(XTRKCAD_USE_GTK)
	if(XTRKCAD_GTK3_EXPERIMENT)
		add_subdirectory(gtk3lib)
	else()
		add_subdirectory(gtklib)
	endif()
else()
	add_subdirectory(mswlib)
endif()

