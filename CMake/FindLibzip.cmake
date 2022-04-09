# Finds libzip.
#
# This module defines:
# LIBZIP_INCLUDE_DIR_ZIP
# LIBZIP_INCLUDE_DIR_ZIPCONF
# LIBZIP_LIBRARY
#
# There is no default installation for libzip on Windows so a
# XTrackCAD specific directory tree is assumed
#

if(WIN32)
  # Folders for x86/x64
  if (WIN64)
    find_path( LIBZIP_INCLUDE_DIR_ZIP zip.h
      PATHS
      $ENV{XTCEXTERNALROOT}/x64/libzip
      DOC "The directory where zip.h resides")
    find_path( LIBZIP_INCLUDE_DIR_ZIPCONF zipconf.h
      PATHS
      $ENV{XTCEXTERNALROOT}/x64/libzip
      DOC "The directory where zip.h resides")
    find_library( LIBZIP_LIBRARY
      NAMES zip Zip
      PATHS
      $ENV{XTCEXTERNALROOT}/x64/libzip
      DOC "The libzip library")
    find_file( LIBZIP_SHAREDLIB
      NAMES zip.dll Zip.dll
      PATHS
      $ENV{XTCEXTERNALROOT}/x64/libzip)
  else (WIN64)
    find_path( LIBZIP_INCLUDE_DIR_ZIP zip.h
      PATHS
      $ENV{XTCEXTERNALROOT}/x86/libzip
      DOC "The directory where zip.h resides")
    find_path( LIBZIP_INCLUDE_DIR_ZIPCONF zipconf.h
      PATHS
      $ENV{XTCEXTERNALROOT}/x86/libzip
      DOC "The directory where zip.h resides")
    find_library( LIBZIP_LIBRARY
      NAMES zip Zip
      PATHS
      $ENV{XTCEXTERNALROOT}/x86/libzip
      DOC "The libzip library")
    find_file( LIBZIP_SHAREDLIB
      NAMES zip.dll Zip.dll
      PATHS
      $ENV{XTCEXTERNALROOT}/x86/libzip)
  endif (WIN64)
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
else(UNIX AND NOT APPLE)
  find_library(LIBZIP_LIBRARY
    NAMES zip)
endif(UNIX AND NOT APPLE)
endif(WIN32)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
    Libzip DEFAULT_MSG
    LIBZIP_LIBRARY LIBZIP_INCLUDE_DIR_ZIP LIBZIP_INCLUDE_DIR_ZIPCONF)

set(LIBZIP_VERSION 0)

if (LIBZIP_INCLUDE_DIR_ZIPCONF)
  FILE(READ "${LIBZIP_INCLUDE_DIR_ZIPCONF}/zipconf.h" _LIBZIP_VERSION_CONTENTS)
  if (_LIBZIP_VERSION_CONTENTS)
    STRING(REGEX REPLACE ".*#define LIBZIP_VERSION \"([0-9a-z.]+)\".*" "\\1" LIBZIP_VERSION "${_LIBZIP_VERSION_CONTENTS}")
  endif ()
endif ()

set(LIBZIP_VERSION ${LIBZIP_VERSION} CACHE STRING "Version number of libzip")
mark_as_advanced(LIBZIP_LIBRARY LIBZIP_INCLUDE_DIR_ZIP LIBZIP_INCLUDE_DIR_ZIPCONF LIBZIP_SHAREDLIB) 
