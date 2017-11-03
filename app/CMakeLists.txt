# "bin/i18n.h" is widely used
INCLUDE_DIRECTORIES(bin)
include_directories(dynstring)
include_directories(cornu)

# Setup the rest of the build ...
add_subdirectory(dynstring)
ADD_SUBDIRECTORY(wlib)
ADD_SUBDIRECTORY(tools)
ADD_SUBDIRECTORY(help)
ADD_SUBDIRECTORY(doc)
ADD_SUBDIRECTORY(bin)
ADD_SUBDIRECTORY(lib)
ADD_SUBDIRECTORY(cornu)

IF(XTRKCAD_USE_GETTEXT)
	ADD_SUBDIRECTORY(i18n)
ENDIF(XTRKCAD_USE_GETTEXT)

