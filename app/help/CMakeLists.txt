project(help)

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
# target_sources(genhelp PUBLIC
# 	genhelp.c
# )

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
	set(GENHELP_OPTS "-bhi" PARENT_SCOPE )
	set(GENMESSAGES_OPTS "-i18n")
else(XTRKCAD_USE_GETTEXT)
	set(GENHELP_OPTS "-bh" PARENT_SCOPE )
	set(GENMESSAGES_OPTS "")
endif(XTRKCAD_USE_GETTEXT)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/messages.h ${CMAKE_CURRENT_BINARY_DIR}/messages.but
	DEPENDS genmessages ${CMAKE_CURRENT_SOURCE_DIR}/messages.in
	COMMAND genmessages ${GENMESSAGES_OPTS} ${CMAKE_CURRENT_SOURCE_DIR}/messages.in ${CMAKE_CURRENT_BINARY_DIR}/messages.but
	)

add_custom_target(Help ALL
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/messages.h ${CMAKE_CURRENT_BINARY_DIR}/messages.but
	)

#
install(
	FILES xtrkcad.tip
	DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}
	)
