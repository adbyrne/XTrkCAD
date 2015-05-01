# Automatically generate most of our external dependencies ...
GET_TARGET_PROPERTY(XTRKCAD_LOCATION xtrkcad LOCATION)
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/dependencies.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/dependencies.cmake" @ONLY)
INSTALL(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/dependencies.cmake")

# Handle some external dependencies that can't be detected automatically ...
SET(MACPORTS_DIR "/opt/local" CACHE PATH "Root directory containing installed MacPort packages")
MARK_AS_ADVANCED(MACPORTS_DIR)

INSTALL(DIRECTORY
  ${MACPORTS_DIR}/lib/gtk-2.0
  DESTINATION lib
  USE_SOURCE_PERMISSIONS
  )

INSTALL(DIRECTORY
  ${MACPORTS_DIR}/etc/gtk-2.0
  DESTINATION etc
  USE_SOURCE_PERMISSIONS
  )

INSTALL(DIRECTORY
  ${MACPORTS_DIR}/lib/pango
  DESTINATION lib
  USE_SOURCE_PERMISSIONS
  )

INSTALL(DIRECTORY
  ${MACPORTS_DIR}/lib/gdk-pixbuf-2.0
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

INSTALL(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/yosemite-fix/libxml2.2.dylib
  ${CMAKE_CURRENT_SOURCE_DIR}/yosemite-fix/libz.1.dylib
  ${CMAKE_CURRENT_SOURCE_DIR}/yosemite-fix/libiconv.2.dylib
  DESTINATION lib
)

# Generate bundles ...
ADD_SUBDIRECTORY(bundle)

