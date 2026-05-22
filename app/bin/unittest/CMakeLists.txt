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

add_executable(shortentest
			  shortentest.c
			  ../shortentext.c
			 )

target_include_directories(shortentest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(shortentest
					PRIVATE
					${LIBS})

add_test(ShortenTest shortentest)

# app defaults

add_executable(defaultstest
			  defaultstest.c
			  )

target_include_directories(defaultstest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/app/wlib/include
)

target_link_libraries(defaultstest
					  ${LIBS})

add_test(DefaultsTest defaultstest)

# geometric utility functions

add_executable(utilitytest
			  utilitytest.c
			  )

target_include_directories(utilitytest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_SOURCE_DIR}/app/wlib/include
)

target_link_libraries(utilitytest ${LIBS})
if(UNIX)
    target_link_libraries(utilitytest m)
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

target_link_libraries(beziertest ${LIBS})
if(UNIX)
    target_link_libraries(beziertest m)
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

target_link_libraries(cornutest ${LIBS})
if(UNIX)
    target_link_libraries(cornutest m)
endif()

add_test(CornuTest cornutest)

set_target_properties(
	dxfformattest
	shortentest
	defaultstest
	pathstest
	utilitytest
	beziertest
	cornutest
 	PROPERTIES FOLDER UnitTests
 )

 if(WIN32 AND CMOCKA_DLL)
	add_custom_command(
		TARGET dxfformattest POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy
				"${CMOCKA_DLL}"
				${CMAKE_CURRENT_BINARY_DIR}
	)
 endif()
