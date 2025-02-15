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
