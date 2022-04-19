
# "bin/i18n.h" is widely used
include_directories(bin 
					)

include_directories(libzip)

# Setup the rest of the build ...
add_subdirectory(cornu)
add_subdirectory(dynstring)
ADD_SUBDIRECTORY(wlib)
add_subdirectory(tools)
ADD_SUBDIRECTORY(help)
ADD_SUBDIRECTORY(doc)
ADD_SUBDIRECTORY(bin)
ADD_SUBDIRECTORY(lib)

if(XTRKCAD_USE_GETTEXT)
	add_subdirectory(i18n)
endif(XTRKCAD_USE_GETTEXT)