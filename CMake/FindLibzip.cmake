# Finds libzip.
#
# This module defines:
# LIBZIP_INCLUDE_DIR_ZIP
# LIBZIP_INCLUDE_DIR_ZIPCONF
# LIBZIP_LIBRARY
# LIBZIP_VERSION
#
# Target Libzip::Libzip is exported
#
# There is no default installation for libzip on Windows so a
# XTrackCAD specific directory tree is assumed
#

if(WIN32)
  set(LIBZIPBASEDIR "$ENV{XTCEXTERNALROOT}/${XTRKCAD_ARCH_SUBDIR}/libzip" )
  find_path( LIBZIP_INCLUDE_DIR_ZIP zip.h
      PATHS ${LIBZIPBASEDIR} "${LIBZIPBASEDIR}/include" 
      DOC "The directory where zip.h resides")
  find_path( LIBZIP_INCLUDE_DIR_ZIPCONF zipconf.h
      PATHS ${LIBZIPBASEDIR} "${LIBZIPBASEDIR}/include"
      DOC "The directory where zip.h resides")
  find_library( LIBZIP_LIBRARY
      NAMES zip Zip 
      PATHS ${LIBZIPBASEDIR} "${LIBZIPBASEDIR}/lib"
      DOC "The libzip library")
else(WIN32)
  find_package(PkgConfig)
  pkg_check_modules(PC_LIBZIP QUIET libzip)

  find_path(LIBZIP_INCLUDE_DIR_ZIP
    NAMES zip.h
    HINTS ${PC_LIBZIP_INCLUDE_DIRS})

  find_path(LIBZIP_INCLUDE_DIR_ZIPCONF
    NAMES zipconf.h
    HINTS ${PC_LIBZIP_INCLUDE_DIRS})

  if(UNIX AND NOT APPLE)
    find_library(LIBZIP_LIBRARY
      NAMES libzip.a zip)
  else()
    find_library(LIBZIP_LIBRARY
      NAMES zip)
  endif()
endif(WIN32)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Libzip 
    REQUIRED_VARS
    LIBZIP_LIBRARY LIBZIP_INCLUDE_DIR_ZIP LIBZIP_INCLUDE_DIR_ZIPCONF
)

if(Libzip_FOUND)
  mark_as_advanced(
    LIBZIP_LIBRARY 
    LIBZIP_INCLUDE_DIR_ZIP 
    LIBZIP_INCLUDE_DIR_ZIPCONF 
  ) 
endif()  

set(LIBZIP_VERSION 0)

if(Libzip_FOUND AND NOT TARGET Libzip::Libzip)
  add_library(Libzip::Libzip UNKNOWN IMPORTED)
  set_property(TARGET Libzip::Libzip PROPERTY IMPORTED_LOCATION ${LIBZIP_LIBRARY})
  target_include_directories(Libzip::Libzip INTERFACE ${LIBZIP_INCLUDE_DIR_ZIP})

  if (LIBZIP_INCLUDE_DIR_ZIPCONF)
    file(READ "${LIBZIP_INCLUDE_DIR_ZIPCONF}/zipconf.h" _LIBZIP_VERSION_CONTENTS)
    if (_LIBZIP_VERSION_CONTENTS)
      string(REGEX REPLACE ".*#define LIBZIP_VERSION \"([0-9a-z.]+)\".*" "\\1" LIBZIP_VERSION "${_LIBZIP_VERSION_CONTENTS}")
    endif ()
  endif ()
  set(LIBZIP_VERSION ${LIBZIP_VERSION} CACHE STRING "Version number of libzip")
endif()
