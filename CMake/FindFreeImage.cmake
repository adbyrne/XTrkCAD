#
# Try to find the FreeImage library and include path.
# Once done this will define
#
# FREEIMAGE_FOUND
# FREEIMAGE_INCLUDE_PATH
# FREEIMAGE_LIBRARY
# FREEIMAGE_SHAREDLIB (Win32 only)
#
# There is no default installation for FreeImage on Windows so a
# XTrackCAD specific directory tree is assumed
#

if(WIN32)
    # Folders for x86/x64
	set( FREEIMAGEBASEDIR  "$ENV{XTCEXTERNALROOT}/${XTRKCAD_ARCH_SUBDIR}/FreeImage" )
	find_path( FREEIMAGE_INCLUDE_PATH FreeImage.h
		PATHS ${FREEIMAGEBASEDIR}
		DOC "The directory where FreeImage.h resides")
	find_library( FREEIMAGE_LIBRARY
		NAMES FreeImage freeimage
		PATHS ${FREEIMAGEBASEDIR}
		DOC "The FreeImage library")
	find_file( FREEIMAGE_SHAREDLIB
		NAMES freeimage.DLL
		PATHS ${FREEIMAGEBASEDIR}
	)
else()
	find_path( FREEIMAGE_INCLUDE_PATH FreeImage.h
		/usr/include
		/usr/local/include
		/sw/include
		/opt/local/include
		DOC "The directory where FreeImage.h resides")
	find_library( FREEIMAGE_LIBRARY
		NAMES FreeImage freeimage
		PATHS
		/usr/lib64
		/usr/lib
		/usr/local/lib64
		/usr/local/lib
		/sw/lib
		/opt/local/lib
		DOC "The FreeImage library")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args( FreeImage
		REQUIRED_VARS
		FREEIMAGE_LIBRARY
		FREEIMAGE_INCLUDE_PATH
)

if(FreeImage_FOUND)
	mark_as_advanced(
		FREEIMAGE_FOUND
		FREEIMAGE_LIBRARY
		FREEIMAGE_INCLUDE_PATH
  		FREEIMAGE_SHAREDLIB
	)
endif()

if(FreeImage_FOUND AND NOT TARGET FreeImage::FreeImage)
	add_library(FreeImage::FreeImage UNKNOWN IMPORTED)
	set_property(TARGET FreeImage::FreeImage PROPERTY IMPORTED_LOCATION ${FREEIMAGE_LIBRARY})
	target_include_directories(FreeImage::FreeImage INTERFACE ${FREEIMAGE_INCLUDE_PATH})

	SET(FreeImage_VERSION_MAJOR)
    SET(FreeImage_VERSION_MINOR)
    SET(FreeImage_VERSION_PATCH)
    FILE(READ "${FREEIMAGE_INCLUDE_PATH}/FreeImage.h" _FreeImage_H_CONTENTS)

    STRING(REGEX MATCH "#define[ \t]+FREEIMAGE_MAJOR_VERSION[ \t]+[0-9]+" FreeImage_VERSION_MAJOR "${_FreeImage_H_CONTENTS}")
    STRING(REGEX MATCH "[0-9]+$" FreeImage_VERSION_MAJOR ${FreeImage_VERSION_MAJOR})

    STRING(REGEX MATCH "#define[ \t]+FREEIMAGE_MINOR_VERSION[ \t]+[0-9]+" FreeImage_VERSION_MINOR "${_FreeImage_H_CONTENTS}")
    STRING(REGEX MATCH "[0-9]+$" FreeImage_VERSION_MINOR ${FreeImage_VERSION_MINOR})

    STRING(REGEX MATCH "#define[ \t]+FREEIMAGE_RELEASE_SERIAL[ \t]+[0-9]+" FreeImage_VERSION_PATCH
           "${_FreeImage_H_CONTENTS}")
    STRING(REGEX MATCH "[0-9]+$" FreeImage_VERSION_PATCH ${FreeImage_VERSION_PATCH})

	SET(FreeImage_VERSION "${FreeImage_VERSION_MAJOR}.${FreeImage_VERSION_MINOR}.${FreeImage_VERSION_PATCH}" 
		CACHE STRING 
		"Version number of FreeImage")
endif()
