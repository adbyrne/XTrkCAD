# - Try to find CMocka
# Once done this will define
#
#  CMOCKA_ROOT_DIR - Set this variable to the root installation of CMocka
#
# Read-Only variables:
#  CMOCKA_FOUND - system has CMocka
#  CMOCKA_INCLUDE_DIR - the CMocka include directory
#  CMOCKA_LIBRARIES - Link these to use CMocka
#  CMOCKA_DEFINITIONS - Compiler switches required for using CMocka
#
#=============================================================================
#  Copyright (c) 2011-2012 Andreas Schneider <asn@cryptomilk.org>
#
#  Distributed under the OSI-approved BSD License (the "License");
#  see accompanying file Copyright.txt for details.
#
#  This software is distributed WITHOUT ANY WARRANTY; without even the
#  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#  See the License for more information.
#=============================================================================
#

set(CMOCKA_ROOT_DIR
	"$ENV{XTCEXTERNALROOT}/${XTRKCAD_ARCH_SUBDIR}/cmocka")

find_path(CMOCKA_INCLUDE_DIR
    NAMES
        cmocka.h
    PATHS
        ${CMOCKA_ROOT_DIR}/include
    PATH_SUFFIXES
        cmocka
)

find_library(CMOCKA_LIBRARY
    NAMES
        cmocka-static
        cmocka
    PATHS
        ${CMOCKA_ROOT_DIR}/lib
)

find_file(CMOCKA_DLL
    NAMES cmocka.dll
    PATHS ${CMOCKA_ROOT_DIR}
    PATH_SUFFIXES bin lib
)

if (CMOCKA_LIBRARY)
  set(CMOCKA_LIBRARIES
      ${CMOCKA_LIBRARIES}
      ${CMOCKA_LIBRARY}
  )
endif (CMOCKA_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CMocka DEFAULT_MSG CMOCKA_LIBRARY CMOCKA_INCLUDE_DIR)

# show the CMOCKA_INCLUDE_DIR and CMOCKA_LIBRARIES variables only in the advanced view
mark_as_advanced(CMOCKA_INCLUDE_DIR CMOCKA_LIBRARY CMOCKA_LIBRARIES CMOCKA_DLL)

# Target CMocka::cmocka carries the library and its include directory
if(CMOCKA_FOUND AND NOT TARGET CMocka::cmocka)
  add_library(CMocka::cmocka UNKNOWN IMPORTED)
  set_property(TARGET CMocka::cmocka PROPERTY IMPORTED_LOCATION ${CMOCKA_LIBRARY})
  target_include_directories(CMocka::cmocka INTERFACE ${CMOCKA_INCLUDE_DIR})
endif()
