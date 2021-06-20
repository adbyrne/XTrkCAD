#
# Try to find the mini-xml library and include path.
# Once done this will define
#
# MINIXML_FOUND
# MINIXML_INCLUDE_PATH
# MINIXML_LIBRARY
# MINIXML_SHAREDLIB (Win32 only)
#
# There is no default installation for mini-xml on Windows so a
# XTrackCAD specific directory tree is assumed
#

if (WIN32)
	find_path( MINIXML_INCLUDE_PATH mxml.h
		PATHS
    $ENV{XTCEXTERNALROOT}/x86/mxml-3.1
	DOC "The directory where mxml.h resides")
	find_library( MINIXML_LIBRARY
		NAMES mxml1
		PATHS
    $ENV{XTCEXTERNALROOT}/x86/mxml-3.1
	  DOC "The Mini XML shared library")
	find_file( MINIXML_SHAREDLIB
		NAMES mxml1.DLL
		PATHS
    $ENV{XTCEXTERNALROOT}/x86/mxml-3.1
	  DOC "The Mini XML DLL" )
  find_library( MINIXML_STATIC_LIBRARY
	  NAMES mxmlstat.lib
	  PATHS
	  $ENV{XTCEXTERNALROOT}/x86/mxml-3.1
	  DOC "The Mini XML static library")
else (WIN32)
	find_path( MINIXML_INCLUDE_PATH mxml.h
		/usr/include
		/usr/local/include
		/sw/include
		/opt/local/include
	DOC "The directory where mxml.h resides")
	find_library( MINIXML_LIBRARY
		NAMES mxml1 mxml
		PATHS
		/usr/lib64
		/usr/lib
		/usr/local/lib64
		/usr/local/lib
		/sw/lib
		/opt/local/lib
	DOC "The Mini XML library")
endif (WIN32)

find_package_handle_standard_args( MiniXML
		DEFAULT_MSG
		MINIXML_LIBRARY
		MINIXML_INCLUDE_PATH
)

mark_as_advanced(
	MINIXML_FOUND
	MINIXML_LIBRARY
	MINIXML_SHAREDLIB
MINIXML_INCLUDE_PATH)
