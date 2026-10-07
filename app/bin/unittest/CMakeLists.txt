# build unit tests for the xtrkcad library
#

# dxf formatting

add_executable(dxfformattest
				dxfformattest.c
				../dxfformat.c
			  )

target_include_directories(dxfformattest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(dxfformattest
                      PRIVATE
                      dynstring
					 ${LIBS})

add_test(DXFOutputTest dxfformattest)

# path manipulation

add_executable( pathstest
				pathstest.c
				)

target_include_directories(pathstest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/app/wlib/include
)

target_link_libraries(pathstest
                      PRIVATE
                      dynstring
					  xtrkcad-wlib
					  ${LIBS})

add_test(PathsTest pathstest)

# handling of default settings

add_executable( defaultstest
				defaultstest.c
				)

target_include_directories(defaultstest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/app/wlib/include
)

target_link_libraries(defaultstest PRIVATE
					  ${LIBS})

add_test(DefaultsTest defaultstest)

#

add_executable(shortentest
			  shortentest.c
			  ../shortentext.c
			 )

target_include_directories(shortentest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(shortentest
					PRIVATE
					${LIBS})

add_test(ShortenTest shortentest)

# geometric utility functions

add_executable(utilitytest
			  utilitytest.c
			  )

target_include_directories(utilitytest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/app/wlib/include
)

target_link_libraries(utilitytest PRIVATE ${LIBS})
if(UNIX)
    target_link_libraries(utilitytest PRIVATE m)
endif()

add_test(UtilityTest utilitytest)

# Bezier pure-math functions

add_executable(beziertest
			  beziertest.c
			  )

target_include_directories(beziertest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/app/wlib/include
)

target_link_libraries(beziertest PRIVATE ${LIBS})
if(UNIX)
    target_link_libraries(beziertest PRIVATE m)
endif()

add_test(BezierTest beziertest)

# Cornu pure-math functions

add_executable(cornutest
			  cornutest.c
			  )

target_include_directories(cornutest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/app/wlib/include
)

target_link_libraries(cornutest PRIVATE ${LIBS})
if(UNIX)
    target_link_libraries(cornutest PRIVATE m)
endif()

add_test(CornuTest cornutest)

# undo stream buffer operations

add_executable(undostreamtest
	undostreamtest.c
)

target_include_directories(undostreamtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/app/wlib/include
)

target_link_libraries(undostreamtest PRIVATE ${LIBS})

add_test(UndoStreamTest undostreamtest)

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
	utilitytest
	beziertest
	cornutest
	undostreamtest
 	PROPERTIES FOLDER UnitTests
 )

if(XTRKCAD_SANITIZE AND UNIX)
	set(SANITIZE_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
	foreach(t dxfformattest shortentest defaultstest pathstest utilitytest beziertest cornutest undostreamtest)
		target_compile_options(${t} PRIVATE ${SANITIZE_FLAGS})
		target_link_options(${t} PRIVATE ${SANITIZE_FLAGS})
	endforeach()
endif()