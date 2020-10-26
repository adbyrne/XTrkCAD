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

add_executable(shortentest
			  shortentest.c
			  ../shortentext.c
			 )

target_link_libraries(shortentest
					${LIBS})

add_test(ShortenTest shortentest)

add_test(CatalogTest catalogtest)

set (TESTXTP 
	"atl83ho.xtp" "atlasn.xtp" "HO-Peco-Code83.xtp"
	)

foreach(testfile IN LISTS TESTXTP )
	configure_file ( ${CMAKE_CURRENT_SOURCE_DIR}/testfiles/${testfile}
                ${CMAKE_CURRENT_BINARY_DIR}
				COPYONLY )
endforeach()

add_executable(catalogtest
				catalogtest.c
				../partcatalog.c
				../paths.c
				)

target_link_libraries(catalogtest
					dynstring
					${LIBS})