#
# Try to find the mini-xml library and include path.
# Once done this will define
#
# MINIXML_FOUND
# MINIXML_INCLUDE_PATH
# MINIXML_LIBRARY
#
# On Windows, MSVC builds look in an XTrackCAD specific directory tree
# ($XTCEXTERNALROOT) for the static mxmlstat.lib. MSYS2 builds find the mxml
# package's libmxml.a in the MSYS2 prefix; the static library is preferred, so
# no extra DLL has to be shipped (Development Issues #96).

# Mini-XML 4 installs beside 3, as libmxml4/mxml.h and libmxml4 (Debian
# forky, Homebrew; Development Issues #99). The header comes first, below
# include/ or include/libmxml4/ (mxml 3 first where both are installed),
# and its MXML_MAJOR_VERSION picks the library names, so the two match.
if (WIN32)
	set(MXMLBASEDIR "$ENV{XTCEXTERNALROOT}/${XTRKCAD_ARCH_SUBDIR}/mxml" )
	find_path( MINIXML_INCLUDE_PATH mxml.h
		PATHS ${MXMLBASEDIR}
		PATH_SUFFIXES libmxml4
		DOC "The directory where mxml.h resides")
else ()
	find_path( MINIXML_INCLUDE_PATH mxml.h
		PATHS
		/usr/include
		/usr/local/include
		/sw/include
		/opt/local/include
		/app/include
		PATH_SUFFIXES libmxml4
		DOC "The directory where mxml.h resides")
endif ()

set(_mxml_major 3)
if(MINIXML_INCLUDE_PATH AND EXISTS "${MINIXML_INCLUDE_PATH}/mxml.h")
	file(STRINGS "${MINIXML_INCLUDE_PATH}/mxml.h" _mxml_major_line
		REGEX "#[ \t]*define[ \t]+MXML_MAJOR_VERSION[ \t]+[0-9]+")
	if(_mxml_major_line MATCHES "([0-9]+)")
		set(_mxml_major "${CMAKE_MATCH_1}")
	endif()
endif()

# static libraries first, so no extra shared library has to be shipped
if(_mxml_major GREATER_EQUAL 4)
	set(_mxml_names libmxml4.a mxml4)
elseif(WIN32)
	set(_mxml_names mxmlstat.lib libmxml.a mxml)
else()
	set(_mxml_names libmxml.a mxml1 mxml)
endif()

if (WIN32)
	find_library( MINIXML_LIBRARY
		NAMES ${_mxml_names}
		PATHS ${MXMLBASEDIR}
		DOC "The Mini XML static library")
else ()
	find_library( MINIXML_LIBRARY
		NAMES ${_mxml_names}
		PATHS
		/usr/lib64
		/usr/lib
		/usr/local/lib64
		/usr/local/lib
		/sw/lib
		/opt/local/lib
		/app/lib
		DOC "The Mini XML library")
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
		MINIXML_INCLUDE_PATH
	)
endif()

if (MiniXML_FOUND AND NOT TARGET MiniXML::mxml)
  add_library(MiniXML::mxml UNKNOWN IMPORTED)
  set_property(TARGET MiniXML::mxml PROPERTY IMPORTED_LOCATION ${MINIXML_LIBRARY})
  target_include_directories(MiniXML::mxml INTERFACE ${MINIXML_INCLUDE_PATH})
  # The static library uses pthreads but doesn't carry that dependency: the
  # Clang linker on Windows ARM64 (CLANGARM64) can't find pthread_once etc.
  # without it (Development Issues #96)
  if(MINIXML_LIBRARY MATCHES "\\.a$")
    find_package(Threads)
    if(Threads_FOUND)
      target_link_libraries(MiniXML::mxml INTERFACE Threads::Threads)
    endif()
  endif()

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
