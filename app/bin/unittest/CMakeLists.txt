# build unit tests for the xtrkcad library

add_executable(dxfformattest 
				dxfformattest.c
				../dxfformat.c
			  )
			  
target_link_libraries(dxfformattest
                      dynstring
					 ${LIBS})

add_test(DXFOutputTest dxfformattest)