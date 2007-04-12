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

INCLUDE_DIRECTORIES(${wlib_SOURCE_DIR}/include)
ADD_LIBRARY(xtrackcad-help ${CMAKE_CURRENT_BINARY_DIR}/messages.h ${CMAKE_CURRENT_BINARY_DIR}/bllnhlp.c)

