PROJECT(doc)

IF (UNIX)
	SET( butfiles ${CMAKE_CURRENT_SOURCE_DIR}/linconf.but )
ELSE (UNIX)
	SET( butfiles ${CMAKE_CURRENT_SOURCE_DIR}/chmconf.but )
ENDIF (UNIX)	

SET( butfiles ${butfiles}
				  ${CMAKE_CURRENT_SOURCE_DIR}/intro.but 
				  ${CMAKE_CURRENT_SOURCE_DIR}/mainw.but 
				  ${CMAKE_CURRENT_SOURCE_DIR}/filem.but 
				  ${CMAKE_CURRENT_SOURCE_DIR}/editm.but 
				  ${CMAKE_CURRENT_SOURCE_DIR}/viewm.but 
				  ${CMAKE_CURRENT_SOURCE_DIR}/addm.but 
				  ${CMAKE_CURRENT_SOURCE_DIR}/changem.but 
				  ${CMAKE_CURRENT_SOURCE_DIR}/drawm.but 
				  ${CMAKE_CURRENT_SOURCE_DIR}/managem.but 
				  ${CMAKE_CURRENT_SOURCE_DIR}/optionm.but 
				  ${CMAKE_CURRENT_SOURCE_DIR}/win_help.but 
				  ${CMAKE_CURRENT_SOURCE_DIR}/map_hot.but 
				  ${CMAKE_CURRENT_SOURCE_DIR}/appendix.but )

ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/index.html
	DEPENDS ${butfiles}
	COMMAND ../tools/halibut/halibut ${butfiles}
	)

ADD_CUSTOM_TARGET( htmlhelp ALL
						  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/index.html )
