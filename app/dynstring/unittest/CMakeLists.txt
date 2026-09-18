# build unit tests for the dynstring library

add_executable(dynstringtest DynStringTest.c)
target_link_libraries(dynstringtest 
    PRIVATE
		  dynstring
      ${CMOCKA_LIBRARIES}
)

add_test(DynStringTest dynstringtest)

set_target_properties(
	dynstringtest 
 	PROPERTIES FOLDER UnitTests
 )