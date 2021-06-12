string(REPLACE "/" "\\\\" NATIVE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(CPACK_GENERATOR "NSIS")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "XTrackCAD")
set(CPACK_NSIS_EXECUTABLE_NAME "bin\\\\xtrkcad.exe")
set(CPACK_NSIS_DISPLAY_NAME "XTrackCAD ${CPACK_PACKAGE_VERSION}")
set(CPACK_PACKAGE_ICON "${NATIVE_CURRENT_SOURCE_DIR}\\\\headerimage.bmp")
set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\xtrkcad.exe")
set(CPACK_NSIS_HELP_LINK "http://www.xtrkcad.org")
set(CPACK_NSIS_MUI_FINISHPAGE_RUN "xtrkcad.exe")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL "on")

set(CPACK_NSIS_STARTMENU_FOLDER "XTrackCAD")

set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
	"; add part specific to XTrackCAD installation
	!include \\\"${NATIVE_CURRENT_SOURCE_DIR}\\\\install.nsh\\\"")

set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
	"; add part specific to XTrackCAD uninstall
	!include \\\"${NATIVE_CURRENT_SOURCE_DIR}\\\\uninstall.nsh\\\"")

include (InstallRequiredSystemLibraries)

include(CPack)
