# Setup packaging ...
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "XTrackCAD is a CAD program for designing model railroad layouts.")
SET(CPACK_GENERATOR "RPM" "STGZ")
SET(CPACK_SOURCE_GENERATOR "TGZ" "TBZ2" )
SET(CPACK_PACKAGE_RELEASE 1)
SET(CPACK_RPM_PACKAGE_LICENSE "GPL")
SET(CPACK_RPM_PACKAGE_GROUP "Applications/Engineering")
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}////xtrkcad-desc.txt")
SET(CPACK_PACKAGE_FILE_NAME "xtrkcad-setup-${XTRKCAD_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")

SET(CPACK_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} )
SET(CPACK_PACKAGING_INSTALL_PREFIX ${CPACK_INSTALL_PREFIX})

# get the specific configuration for each packager
SET(CPACK_PROJECT_CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/package.txt)

SET(CPACK_PACKAGE_VENDOR "XTrackCAD Fork Project")
SET(CPACK_RPM_PACKAGE_REQUIRES "glibc gtk2 webkitgtk")
# exclude these directories from the  rpm
SET(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
	"/usr/local"
	"/usr/local/share"
	"/usr/local/bin" 
	"/usr/share/locale"
	"/usr/share/locale/*"
	"/usr/local/share/locale"
	"/usr/local/share/locale/*"
)

SET(CPACK_RPM_CHANGELOG_FILE "${CMAKE_CURRENT_SOURCE_DIR}////changelog.txt")

INCLUDE(CPack)

