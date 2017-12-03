# Setup packaging ...
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "XTrackCAD is a CAD program for designing model railroad layouts.")
set(CPACK_GENERATOR "RPM" "STGZ")
set(CPACK_SOURCE_GENERATOR "TGZ" "TBZ2" "ZIP")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_RPM_PACKAGE_LICENSE "GPL")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Engineering")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}////xtrkcad-desc.txt")
set(CPACK_PACKAGE_FILE_NAME "xtrkcad-setup-${XTRKCAD_VERSION}-${CPACK_PACKAGE_RELEASE}.${CMAKE_SYSTEM_PROCESSOR}")

set(CPACK_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} )
set(CPACK_PACKAGING_INSTALL_PREFIX ${CPACK_INSTALL_PREFIX})

# get the specific configuration for each packager
set(CPACK_PROJECT_CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/package.txt)

set(CPACK_PACKAGE_VENDOR "XTrackCAD Fork Project")

# common dependencies for all Linux builds
set(CPACK_RPM_PACKAGE_REQUIRES "glibc gtk2")

# Add dependency to webkitgtk if configured
if(NOT XTRKCAD_USE_BROWSER)
	set(CPACK_RPM_PACKAGE_REQUIRES 
	    "${CPACK_RPM_PACKAGE_REQUIRES} webkitgtk")
endif()

# exclude these directories from the rpm
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
	"/usr/local"
	"/usr/local/share"
	"/usr/local/bin" 
	"/usr/share/locale"
	"/usr/share/locale/*"
	"/usr/local/share/locale"
	"/usr/local/share/locale/*"
	"/usr/share/applications"
	"/usr/share/pixmaps"
)

set(CPACK_RPM_CHANGELOG_FILE "${CMAKE_CURRENT_SOURCE_DIR}////changelog.txt")

include(CPack)

