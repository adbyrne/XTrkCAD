# Finds zlib.
#
# This module defines:
# zlib_INCLUDE_DIR_ZIP
# zlib_INCLUDE_DIR_ZIPCONF
# zlib_LIBRARY
#
# There is no default installation for zlib on Windows so a
# XTrackCAD specific directory tree is assumed
#

if(WIN32)
  find_path( ZLIB_INCLUDE_DIR zlib.h
    PATHS
    $ENV{XTCEXTERNALROOT}/x86/zlib
    DOC "The directory where zip.h resides")
  find_library( ZLIB_LIBRARY
    NAMES zlib Zlib
    PATHS
    $ENV{XTCEXTERNALROOT}/x86/zlib
    DOC "The zlib library")
  find_file( ZLIB_SHAREDLIB
    NAMES zlib.dll Zlib.dll
    PATHS
    $ENV{XTCEXTERNALROOT}/x86/zlib)
else(WIN32)
  find_package(PkgConfig)
  pkg_check_modules(PC_ZLIB QUIET zlib)

  find_path(ZLIB_INCLUDE_DIR
      NAMES zlib.h
      HINTS ${PC_ZLIB_INCLUDE_DIRS})

  find_library(ZLIB_LIBRARY
  	NAMES z)
endif(WIN32)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
  ZLIB DEFAULT_MSG
  ZLIB_LIBRARY ZLIB_INCLUDE_DIR)

mark_as_advanced(ZLIB_LIBRARY ZLIB_INCLUDE_DIR)
