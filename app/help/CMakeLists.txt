project(messagefile)

#
# genhelp is used in ..\bin to generate the tooltip texts

add_executable(genhelp "")
target_sources(genhelp 
	PRIVATE
	genhelp.c)

target_link_libraries(genhelp 
	PUBLIC	
	cJSON
)

#
# interpreter for message source file

add_executable(genmessages "")

target_sources(genmessages PUBLIC
	genmessages.c
)

set_source_files_properties(
	genhelp.json messages.in xtrkcad.tip 
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
       OUTPUT ${xtrkcad-lib_BINARY_DIR}/bllnhlp.c
       DEPENDS genhelp genhelp.json
       COMMAND genhelp ${GENHELP_OPTS} ${CMAKE_CURRENT_SOURCE_DIR}/genhelp.json ${xtrkcad-lib_BINARY_DIR}/bllnhlp.c
)

add_custom_target(balloonhelp ALL
	DEPENDS ${xtrkcad-lib_BINARY_DIR}/bllnhlp.c
)

add_custom_command(
 	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/messages.h ${CMAKE_CURRENT_BINARY_DIR}/messages.but
	DEPENDS genmessages ${CMAKE_CURRENT_SOURCE_DIR}/messages.in
 	COMMAND genmessages ${GENMESSAGES_OPTS} ${CMAKE_CURRENT_SOURCE_DIR}/messages.in ${CMAKE_CURRENT_BINARY_DIR}/messages.but
)

add_custom_target(messagefiles ALL
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/messages.h ${CMAKE_CURRENT_BINARY_DIR}/messages.but
)

install(
	FILES xtrkcad.tip
	DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}
)

set_target_properties(
	messagefiles
	balloonhelp
	genmessages
	genhelp
    PROPERTIES FOLDER Appl-Help
)