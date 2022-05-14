include_directories(libzip)

# Setup the rest of the build ...
add_subdirectory(cornu)
add_subdirectory(dynstring)
add_subdirectory(cJSON)
ADD_SUBDIRECTORY(wlib)
add_subdirectory(tools)
add_subdirectory(help)
add_subdirectory(doc)
add_subdirectory(bin)
add_subdirectory(lib)

if(XTRKCAD_USE_GETTEXT)
	add_subdirectory(i18n)
endif(XTRKCAD_USE_GETTEXT)