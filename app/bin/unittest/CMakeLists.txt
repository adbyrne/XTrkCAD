# build unit tests for the xtrkcad library
#

# dxf formatting

add_executable(dxfformattest
				dxfformattest.c
				../dxfformat.c
			  )

target_link_libraries(dxfformattest
                      dynstring
					 ${LIBS})

add_test(DXFOutputTest dxfformattest)

# path manipulation

add_executable( pathstest
				pathstest.c
				)

target_link_libraries(pathstest
                      dynstring
					  xtrkcad-wlib
					  ${LIBS})

add_test(PathsTest pathstest)

# handling of default settings

#add_executable( defaultstest
#				defaultstest.c
#				)

#target_link_libraries(defaultstest
#                      xtrkcad-wlib
#					  ${LIBS})

#add_test(DefaultsTest defaultstest)

#

add_executable(shortentest
			  shortentest.c
			  ../shortentext.c
			 )

target_link_libraries(shortentest
					${LIBS})

add_test(ShortenTest shortentest)

set_target_properties(
	dxfformattest 
	shortentest
#	defaultstest
	pathstest
 	PROPERTIES FOLDER UnitTests
 )

 add_custom_command(
	TARGET dxfformattest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            "${CMOCKA_ROOT_DIR}/lib/cmocka.dll"
            ${CMAKE_CURRENT_BINARY_DIR}
 )

