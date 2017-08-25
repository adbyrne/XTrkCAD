# build unit tests for the xtrkcad library

add_executable(dxfformattest 
				dxfformattest.c
				../dxfformat.c
			  )
			  
target_link_libraries(dxfformattest
                      dynstring
					 ${LIBS})

add_test(DXFOutputTest dxfformattest)

add_executable( pathstest
				pathstest.c
				)

target_link_libraries(pathstest
                      dynstring
					 ${LIBS})

add_test(PathsTest pathstest)

add_executable( defaultstest
				defaultstest.c
				)

target_link_libraries(defaultstest
                      ${LIBS})

add_test(DefaultsTest defaultstest)
