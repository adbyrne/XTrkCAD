# Automatically generate most of our external dependencies ...
GET_TARGET_PROPERTY(XTRKCAD_LOCATION xtrkcad LOCATION)
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/dependencies.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/dependencies.cmake" @ONLY)
INSTALL(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/dependencies.cmake")

# Handle some external dependencies that can't be detected automatically ...
SET(MACPORTS_DIR "/opt/local" CACHE PATH "Root directory containing installed MacPort packages")
MARK_AS_ADVANCED(MACPORTS_DIR)

INSTALL(DIRECTORY
  ${MACPORTS_DIR}/lib/pango
  DESTINATION lib
  USE_SOURCE_PERMISSIONS
  )

INSTALL(FILES
  ${MACPORTS_DIR}/etc/pango/pangorc
  ${MACPORTS_DIR}/etc/pango/pango.modules
  DESTINATION etc/pango
  )

INSTALL(FILES
  ${MACPORTS_DIR}/etc/fonts/fonts.conf
  ${MACPORTS_DIR}/etc/fonts/fonts.dtd
  DESTINATION etc/fonts
  )

# Generate bundles ...
ADD_SUBDIRECTORY(bundle)

