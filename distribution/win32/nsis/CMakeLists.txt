string(REPLACE "/" "\\\\" NATIVE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
string(REPLACE "/" "\\\\" NATIVE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Windows-path form of XTRKCAD_SHARE_INSTALL_DIR ("share/xtrkcad-5.4.0" ->
# "share\xtrkcad-5.4.0") for substitution into install.nsh.in below.
string(REPLACE "/" "\\\\" XTRKCAD_SHARE_INSTALL_DIR_WIN "${XTRKCAD_SHARE_INSTALL_DIR}")

set(CPACK_GENERATOR "NSIS")
# NOTE: CPACK_PACKAGE_INSTALL_DIRECTORY is intentionally NOT set here — it
# inherits the version-qualified value ("XTrkCAD ${XTRKCAD_VERSION}") set in
# distribution/CMakeLists.txt. A fixed "XTrackCAD" override here used to
# make every version install into the same Program Files folder, clobbering
# each other exactly like the .deb/.rpm collision this whole change fixes.
set(CPACK_NSIS_EXECUTABLE_NAME "bin\\\\${XTRKCAD_BIN}.exe")
set(CPACK_NSIS_DISPLAY_NAME "XTrackCAD ${CPACK_PACKAGE_VERSION}")
set(CPACK_PACKAGE_ICON "${NATIVE_CURRENT_SOURCE_DIR}\\\\headerimage.bmp")
set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\${XTRKCAD_BIN}.exe")
set(CPACK_NSIS_HELP_LINK "http://www.xtrkcad.org")
set(CPACK_NSIS_MUI_FINISHPAGE_RUN "${XTRKCAD_BIN}.exe")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL "on")
set(CPACK_NSIS_STARTMENU_FOLDER "XTrackCAD ${XTRKCAD_VERSION}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/install.nsh.in ${CMAKE_CURRENT_BINARY_DIR}/install.nsh @ONLY)

set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
	"; add part specific to XTrackCAD installation
	!include \\\"${NATIVE_CURRENT_BINARY_DIR}\\\\install.nsh\\\"")

set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
	"; add part specific to XTrackCAD uninstall
	!include \\\"${NATIVE_CURRENT_SOURCE_DIR}\\\\uninstall.nsh\\\"")

include(CPack)
