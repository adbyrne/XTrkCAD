PROJECT(help)

ADD_EXECUTABLE(genmessages genmessages.c)
GET_TARGET_PROPERTY(genmessages_EXE genmessages LOCATION)

ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/messages.h
	DEPENDS genmessages ${CMAKE_CURRENT_SOURCE_DIR}/messages.in
	COMMAND ${genmessages_EXE} < ${CMAKE_CURRENT_SOURCE_DIR}/messages.in 
	)

ADD_EXECUTABLE(genhelp genhelp.c)
GET_TARGET_PROPERTY(genhelp_EXE genhelp LOCATION)

ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/bllnhlp.c
	DEPENDS genhelp ${CMAKE_CURRENT_SOURCE_DIR}/genhelp.in
	COMMAND ${genhelp_EXE} -bh ${CMAKE_CURRENT_SOURCE_DIR}/genhelp.in ${CMAKE_CURRENT_BINARY_DIR}/bllnhlp.c
	)

ADD_EXECUTABLE(prochelp prochelp.c readpng.c)
TARGET_LINK_LIBRARIES(prochelp png)
TARGET_LINK_LIBRARIES(prochelp z)
TARGET_LINK_LIBRARIES(prochelp m)
GET_TARGET_PROPERTY(prochelp_EXE prochelp LOCATION)

ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xtrkcad.help
	DEPENDS prochelp ${CMAKE_CURRENT_SOURCE_DIR}/xtrkcad.hlpsrc
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/xtrkcad.hlpsrc ${CMAKE_CURRENT_BINARY_DIR}/xtrkcad.hlpsrc
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/xvprintsel.hlpsrc ${CMAKE_CURRENT_BINARY_DIR}/xvprintsel.hlpsrc
	COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/xvfontsel.hlpsrc ${CMAKE_CURRENT_BINARY_DIR}/xvfontsel.hlpsrc
	COMMAND ${prochelp_EXE} -xv -C unix xtrkcad.hlpsrc xtrkcad.help
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)

ADD_CUSTOM_TARGET(xtrkcad.help ALL
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/xtrkcad.help
	)

INCLUDE_DIRECTORIES(${wlib_SOURCE_DIR}/include)
ADD_LIBRARY(xtrackcad-help ${CMAKE_CURRENT_BINARY_DIR}/messages.h ${CMAKE_CURRENT_BINARY_DIR}/bllnhlp.c)

INSTALL(FILES
	xtrkcad.tip
	${CMAKE_CURRENT_BINARY_DIR}/xtrkcad.help
	DESTINATION share
	)
