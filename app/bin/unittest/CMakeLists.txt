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

add_executable( defaultstest
				defaultstest.c
				)

target_link_libraries(defaultstest
                      xtrkcad-wlib
					  ${LIBS})

add_test(DefaultsTest defaultstest)

#

add_executable(shortentest
			  shortentest.c
			  ../shortentext.c
			 )

target_link_libraries(shortentest
					${LIBS})

add_test(ShortenTest shortentest)

# parts catalog
# disabled because it pulls in too many dependencies

# add_test(CatalogTest catalogtest)

# # parameter files for test cases:
# # - two manufacturers
# # - two scales from same manufacturer

# set (TESTXTP 
# 	"atl83ho.xtp" "atlasn.xtp" "HO-Peco-Code83.xtp"
# 	)

# foreach(testfile IN LISTS TESTXTP )
# 	configure_file ( ${CMAKE_CURRENT_SOURCE_DIR}/testfiles/${testfile}
#                 ${CMAKE_CURRENT_BINARY_DIR}
# 				COPYONLY )
# endforeach()

# add_executable(catalogtest
# 				catalogtest.c
# 				../partcatalog.c
# 				../paths.c
# 				../stringxtc.c
# 				../levenshtein.c
# 				)

# target_link_libraries(catalogtest
# 					dynstring
# 					${CMOCKA_LIBRARIES}
# 					)


set_target_properties(
	dxfformattest 
#	catalogtest
	shortentest
	defaultstest
	pathstest
 	PROPERTIES FOLDER UnitTests
 )