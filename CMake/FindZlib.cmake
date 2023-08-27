# Finds zlib.
#
# This module defines:
# ZLIB_INCLUDE_DIR
# ZLIB_LIBRARY
#
# There is no default installation for zlib on Windows so a
# XTrackCAD specific directory tree is assumed
#

if(WIN32)
    set(ZLIBBASEDIR "$ENV{XTCEXTERNALROOT}/${XTRKCAD_ARCH_SUBDIR}/zlib" )
    find_path( ZLIB_INCLUDE_DIR zlib.h
      PATHS ${ZLIBBASEDIR} "${ZLIBBASEDIR}/include"
      DOC "The directory where zlib.h resides")
    find_library( ZLIB_LIBRARY
      NAMES zlibstatic
      PATHS ${ZLIBBASEDIR} "${ZLIBBASEDIR}/lib"
      DOC "The zlib library")
else()
  find_package(PkgConfig)
  pkg_check_modules(PC_ZLIB QUIET zlib)

  find_path(ZLIB_INCLUDE_DIR
      NAMES zlib.h
      HINTS ${PC_ZLIB_INCLUDE_DIRS})

  find_library(ZLIB_LIBRARY
  	NAMES z)
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    Zlib 
    REQUIRED_VARS
    ZLIB_LIBRARY 
    ZLIB_INCLUDE_DIR
)

if(Zlib_FOUND)
  mark_as_advanced(
    ZLIB_LIBRARY 
    ZLIB_INCLUDE_DIR 
  ) 
endif() 

if(Zlib_FOUND AND NOT TARGET Zlib::Zlib)
  add_library(Zlib::Zlib UNKNOWN IMPORTED)
  set_property(TARGET Zlib::Zlib PROPERTY IMPORTED_LOCATION ${ZLIB_LIBRARY})
  target_include_directories(Zlib::Zlib INTERFACE ${ZLIB_INCLUDE_DIR})

  if (ZLIB_INCLUDE_DIR)
    file(READ "${ZLIB_INCLUDE_DIR}/zlib.h" _ZLIB_VERSION_CONTENTS)
    if (_ZLIB_VERSION_CONTENTS)
      string(REGEX REPLACE ".*#define ZLIB_VERSION \"([0-9a-z.]+)\".*" "\\1" ZLIB_VERSION "${_ZLIB_VERSION_CONTENTS}")
    endif ()
  endif ()
  set(ZLIB_VERSION ${ZLIB_VERSION} CACHE STRING "Version number of zlib")
endif()
