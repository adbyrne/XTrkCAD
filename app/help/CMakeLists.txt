project(messagefile C)

#
# genhelp is used in ..\bin to generate the tooltip texts

add_executable(genhelp "")
target_sources(genhelp 
	PRIVATE
	genhelp.c)

target_link_libraries(genhelp
	PRIVATE
	cJSON
)

#
# interpreter for message source file

add_executable(genmessages "")

target_sources(genmessages PUBLIC
	genmessages.c
)

set_source_files_properties(
	tooltiptext.json messages.in xtrkcad.tip 
	PROPERTIES HEADER_FILE_ONLY TRUE
)

#
# Generate message include file and the input file for the help system

if(XTRKCAD_USE_GETTEXT)
	set(GENHELP_OPTS "-bhi" )
	set(GENMESSAGES_OPTS "-i18n")
else()
	set(GENHELP_OPTS "-bh" )
	set(GENMESSAGES_OPTS "")
endif()

add_custom_command(
       OUTPUT tooltiptext.c
       DEPENDS genhelp tooltiptext.json
       COMMAND $<TARGET_FILE:genhelp> ${GENHELP_OPTS} ${CMAKE_CURRENT_SOURCE_DIR}/tooltiptext.json ${CMAKE_CURRENT_BINARY_DIR}/tooltiptext.c
)

add_custom_target(tooltips ALL
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tooltiptext.c
)

# genmessages.c now emits Doxygen syntax only (halibut's \S{}/\H{} output was
# removed -- see SF feature-requests #219). messages.but is gone;
# app/doc/CMakeLists.txt's still-halibut-based help-html target (which lists
# messages.but as one of its sources) is not updated by this change and will
# fail until that separate, not-yet-done step catches up -- acceptable on
# this branch, which exists only for the Doxygen conversion work.
add_custom_command(
 	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/messages.h ${CMAKE_CURRENT_BINARY_DIR}/messages.dox
	DEPENDS genmessages ${CMAKE_CURRENT_SOURCE_DIR}/messages.in
 	COMMAND $<TARGET_FILE:genmessages> ${GENMESSAGES_OPTS} ${CMAKE_CURRENT_SOURCE_DIR}/messages.in ${CMAKE_CURRENT_BINARY_DIR}/messages.dox
)

add_custom_target(msgfiles ALL
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/messages.h ${CMAKE_CURRENT_BINARY_DIR}/messages.dox
)

install(
	FILES xtrkcad.tip
	DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}
)

set_target_properties(
	msgfiles
	tooltips
	genmessages
	genhelp
    PROPERTIES FOLDER HelpDocs
)