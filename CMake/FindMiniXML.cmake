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
	set(MXMLBASEDIR "$ENV{XTCEXTERNALROOT}/${XTRKCAD_ARCH_SUBDIR}/mxml" )
	find_path( MINIXML_INCLUDE_PATH mxml.h
		PATHS ${MXMLBASEDIR}
		DOC "The directory where mxml.h resides")
	find_library( MINIXML_LIBRARY
		NAMES mxml1
		PATHS ${MXMLBASEDIR}
		DOC "The Mini XML shared library")
	find_file( MINIXML_SHAREDLIB
		NAMES mxml1.DLL
		PATHS ${MXMLBASEDIR}
		DOC "The Mini XML DLL" )
	find_library( MINIXML_STATIC_LIBRARY
		NAMES mxmlstat.lib
		PATHS ${MXMLBASEDIR}
		DOC "The Mini XML static library")
else ()
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
	find_library( MINIXML_STATIC_LIBRARY
		NAMES libmxml.a
		PATHS
		/usr/lib64
		/usr/lib
		/usr/local/lib64
		/usr/local/lib
		/sw/lib
		/opt/local/lib
		DOC "The Mini XML static library")
endif (WIN32)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args( 
	MiniXML
	REQUIRED_VARS
	MINIXML_LIBRARY
	MINIXML_INCLUDE_PATH
)

if(MiniXML_FOUND)
	mark_as_advanced(
		MINIXML_FOUND
		MINIXML_LIBRARY
		MINIXML_SHAREDLIB
		MINIXML_STATIC_LIBRARY
		MINIXML_INCLUDE_PATH
	)
endif()

if (MiniXML_FOUND AND NOT TARGET MiniXML::MiniXML)
  add_library(MiniXML::mxml UNKNOWN IMPORTED)
  set_property(TARGET MiniXML::mxml PROPERTY IMPORTED_LOCATION ${MINIXML_LIBRARY})
  target_include_directories(MiniXML::mxml INTERFACE ${MINIXML_INCLUDE_PATH})

  set(MXML_VERSION_MAJOR)
  set(MXML_VERSION_MINOR)
  file(READ "${MINIXML_INCLUDE_PATH}/mxml.h" _mxml_H_CONTENTS)

  string(REGEX MATCH "#[ \t]*define[ \t]+MXML_MAJOR_VERSION[ \t]+[0-9]+" MXML_VERSION_MAJOR "${_mxml_H_CONTENTS}")
  string(REGEX MATCH "[0-9]+$" MXML_VERSION_MAJOR ${MXML_VERSION_MAJOR})
  
  string(REGEX MATCH "#[ \t]*define[ \t]+MXML_MINOR_VERSION[ \t]+[0-9]+" MXML_VERSION_MINOR "${_mxml_H_CONTENTS}")
  string(REGEX MATCH "[0-9]+$" MXML_VERSION_MINOR ${MXML_VERSION_MINOR})
  set(MiniXML_VERSION "${MXML_VERSION_MAJOR}.${MXML_VERSION_MINOR}" 
	  CACHE STRING
	  "Version number of MiniXML")
endif()