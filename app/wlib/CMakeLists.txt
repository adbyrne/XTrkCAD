project(wlib)

add_library( xtrkcad-wlib
			"")		

add_subdirectory(include)

if(XTRKCAD_USE_GTK)
	add_subdirectory(gtklib)
else(XTRKCAD_USE_GTK)
	add_subdirectory(mswlib)
endif(XTRKCAD_USE_GTK)

