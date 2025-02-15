project(wlib)

add_library( xtrkcad-wlib
			"")		

add_subdirectory(include)

# add i18n.h to include path
target_include_directories(xtrkcad-wlib 
	PRIVATE 
	${xtrkcad-lib_SOURCE_DIR}
)

add_subdirectory(gtk3lib)

add_subdirectory(test)
