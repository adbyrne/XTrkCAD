



# Setup the rest of the build ...

add_subdirectory(help)
add_subdirectory(bin)
add_subdirectory(cornu)
add_subdirectory(dynstring)
add_subdirectory(cJSON)
add_subdirectory(wlib)
add_subdirectory(tools)
add_subdirectory(doc)
add_subdirectory(lib)

# i18n can be found here
include_directories(
	${xtrkcad-lib_SOURCE_DIR}
)

if(XTRKCAD_USE_GETTEXT)
	add_subdirectory(i18n)
endif()