PROJECT(help)

ADD_EXECUTABLE(genhelp genhelp.c)

ADD_EXECUTABLE(genmessages genmessages.c)
GET_TARGET_PROPERTY(genmessages_EXE genmessages LOCATION)

ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/messages.h ${CMAKE_CURRENT_BINARY_DIR}/messages.but
	DEPENDS genmessages ${CMAKE_CURRENT_SOURCE_DIR}/messages.in
	COMMAND ${genmessages_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/messages.in ${CMAKE_CURRENT_BINARY_DIR}/messages.but
	)

ADD_CUSTOM_TARGET(Help ALL
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/messages.h ${CMAKE_CURRENT_BINARY_DIR}/messages.but
	)

INSTALL(FILES xtrkcad.tip DESTINATION share)

