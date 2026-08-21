project(doc C)

# Doxygen is an external tool, unlike the vendored (self-building, always
# available) Halibut compiler this replaced -- most dev/test CI jobs have no
# reason to install it, so this must degrade gracefully rather than fail
# every ordinary configure. Jobs that ship a real package (release.yml's
# package-linux/-macos/-windows/-linux-arm64-*) install Doxygen explicitly
# since the User Guide is a real part of what they package.
find_package(Doxygen QUIET)

if(DOXYGEN_FOUND)
	# intro.dox.in is a template: CMake substitutes "@XTRKCAD_VERSION@" via
	# configure_file() into the real intro.dox that Doxygen compiles, same
	# mechanism this file used for intro.but.in -> intro.but before the
	# Halibut -> Doxygen conversion (SF feature-requests #219, see
	# .claude/halibut-doxygen-investigation-plan.md).
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/intro.dox.in ${CMAKE_CURRENT_BINARY_DIR}/intro.dox @ONLY)

	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

	file(GLOB doc_DOX_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.dox)

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/html/index.html
		DEPENDS
			${doc_DOX_SOURCES}
			${CMAKE_CURRENT_BINARY_DIR}/intro.dox
			${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
			msgfiles
		COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
		COMMENT "Building the XTrkCAD User Guide (Doxygen)"
	)

	add_custom_target(help-html ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/html/index.html)
	set_target_properties(
	   help-html
	   PROPERTIES FOLDER HelpDocs
	)

	install(
		DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
		DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}
	)

	# Apple Help Book packaging (XTRKCAD_USE_APPLEHELP) is a separate, non-default
	# help viewer from the browser-based wHelp() the real release ships on every
	# platform including macOS (XTRKCAD_USE_APPLEHELP=OFF in release.yml, see
	# app/wlib/gtk3lib/CMakeLists.txt) -- only build/install the Help Book bundle
	# when that viewer is actually enabled, not on every APPLE build.
	if (APPLE AND XTRKCAD_USE_APPLEHELP)
		add_custom_command(
			OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/html/XTrackCAD.helpindex
			DEPENDS help-html
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
			COMMAND hiutil -Cf XTrackCAD.helpindex -gva -e "index.html" ${CMAKE_CURRENT_BINARY_DIR}/html
		)

		add_custom_target(help-index ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/html/XTrackCAD.helpindex)

		install(
			DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html/
			DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}/XTrackCAD.help/Contents/Resources/en.lproj
		)
	endif()
else()
	message(STATUS "Doxygen not found -- the User Guide (help-html target) will not be built or installed")
endif()
