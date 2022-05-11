
# "bin/i18n.h" is widely used
include_directories(bin 
					)

include_directories(libzip)

# Setup the rest of the build ...
add_subdirectory(cornu)
add_subdirectory(dynstring)
ADD_SUBDIRECTORY(wlib)
add_subdirectory(tools)
add_subdirectory(help)
ADD_SUBDIRECTORY(doc)
add_subdirectory(bin)
add_subdirectory(lib)

if(XTRKCAD_USE_GETTEXT)
	add_subdirectory(i18n)
endif(XTRKCAD_USE_GETTEXT)