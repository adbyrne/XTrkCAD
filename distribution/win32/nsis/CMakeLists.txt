STRING(REPLACE "/" "\\\\" NATIVE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

SET(CPACK_GENERATOR "NSIS")
SET(CPACK_PACKAGE_ICON "${NATIVE_CURRENT_SOURCE_DIR}\\\\headerimage.bmp")
SET(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\xtrkcad.exe")
SET(CPACK_NSIS_HELP_LINK "http://www.xtrkcad.org")

SET(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
	"; add part specific to XTrackCAD installation
	!include \\\"${NATIVE_CURRENT_SOURCE_DIR}\\\\install.nsh\\\"")
	
SET(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
	"; add part specific to XTrackCAD uninstall
	!include \\\"${NATIVE_CURRENT_SOURCE_DIR}\\\\uninstall.nsh\\\"")
	
INCLUDE(CPack)

