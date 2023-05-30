project(doc)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/intro.but.in ${CMAKE_CURRENT_BINARY_DIR}/intro.but)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/hhc.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/hhc.cmake @ONLY)

#
# Create directory for the html stage

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html)

add_custom_target(helpsystem)
set_target_properties(
   helpsystem
   PROPERTIES FOLDER HelpDocs
)

target_sources(helpsystem
	PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}/intro.but
	${CMAKE_CURRENT_SOURCE_DIR}/addm.but
	${CMAKE_CURRENT_SOURCE_DIR}/changem.but
	${CMAKE_CURRENT_SOURCE_DIR}/drawm.but
	${CMAKE_CURRENT_SOURCE_DIR}/editm.but
	${CMAKE_CURRENT_SOURCE_DIR}/filem.but
	${CMAKE_CURRENT_SOURCE_DIR}/helpm.but
	${CMAKE_CURRENT_SOURCE_DIR}/hotbar.but
	${CMAKE_CURRENT_SOURCE_DIR}/macrom.but
	${CMAKE_CURRENT_SOURCE_DIR}/managem.but
	${CMAKE_CURRENT_SOURCE_DIR}/optionm.but
	${CMAKE_CURRENT_SOURCE_DIR}/statusbar.but
	${CMAKE_CURRENT_SOURCE_DIR}/view_winm.but
	${CMAKE_CURRENT_SOURCE_DIR}/navigation.but
	${CMAKE_CURRENT_SOURCE_DIR}/appendix.but
	${messagefile_BINARY_DIR}/messages.but
	${CMAKE_CURRENT_SOURCE_DIR}/upgrade.but
	${CMAKE_CURRENT_SOURCE_DIR}/warranty.but
)

#
# messages.but is generated in a different directory scope, CMake must not check for existance
set_source_files_properties(${messagefile_BINARY_DIR}/messages.but
	PROPERTIES 
	GENERATED True)

# Add a custom command for cleaning the HTML staging directory
add_custom_target(clean-html
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
	COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/clean-html.cmake
)

# If we're using the GTK back-end, just generate "vanilla" HTML help files for use with gtkhtml
if(XTRKCAD_USE_GTK)

	if (APPLE)
		target_sources(helpsystem
			PRIVATE 
			${CMAKE_CURRENT_SOURCE_DIR}/osxconf.but)
	else()
		target_sources(helpsystem 
			PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}/linconf.but)
	endif()

	get_target_property(halibut_sources helpsystem SOURCES)

	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/html/index.html
		DEPENDS halibut ${halibut_sources}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/clean-html.cmake
		COMMAND halibut ${halibut_sources}
		)

	add_custom_target(help-html ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/html/index.html msgfiles)

	install(
		DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
		DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}
		)

	install(
		DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/png.d
		DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}/html
		)

	install(
		FILES xtrkcad_lin.css
		DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}/html
        )
# Copy the help files to the Help bundle if Apple
	if (APPLE)
        add_custom_command(
        	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/html/XTrackCAD.helpindex
        	DEPENDS help-html ${halibut_sources}
        	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
        	COMMAND hiutil -Cf XTrackCAD.helpindex -gva -e "IndexPage.html" -e "toc.html" ${CMAKE_CURRENT_BINARY_DIR}/html
        )

        add_custom_target(help-index ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/html/XTrackCAD.helpindex)

        install(
            DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html/
            DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}/XTrackCAD.help/Contents/Resources/en.lproj
        )
        install(
            DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/png.d
            DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}/XTrackCAD.help/Contents/Resources/en.lproj
        )
	    install(
	        FILES xtrkcad_osx.css
		    DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}/XTrackCAD.help/Contents/Resources/en.lproj/sty
	)
    endif()

# Otherwise, we're using the Win32 back-end, so generate a compiled HTML help file
else()
	target_sources(helpsystem
		PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}/chmconf.but
	)
	file(GLOB IMAGE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/png.d/*)

	#
	# Use the Microsoft HTML Help Workshop to create a chm file
	# 
	# Clean the working directory
	# Copy the image files and the CSS
	# Create the sources for the Help Workshop using halibut
	# Build the chm file
	# Add the chm file to the installation 
	add_custom_target(help-chm ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/html/index.html)
	set_target_properties(
   		help-chm
   		PROPERTIES FOLDER HelpDocs
	)
	get_target_property(halibut_sources helpsystem SOURCES)
	add_custom_command(
		TARGET help-chm
		DEPENDS halibut helpsystem ${IMAGE_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/xtrkcad_win.css
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/clean-html.cmake
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/png.d ${CMAKE_CURRENT_BINARY_DIR}/html/png.d
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/xtrkcad_win.css ${CMAKE_CURRENT_BINARY_DIR}/html
		COMMAND halibut ${halibut_sources} 
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/hhc.cmake
	)

	install(
		FILES ${CMAKE_CURRENT_BINARY_DIR}/html/xtrkcad.chm
        DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}
		)

endif()
