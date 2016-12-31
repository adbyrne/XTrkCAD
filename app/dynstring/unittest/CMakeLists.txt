# build unit tests for the dynstring library

add_executable(dynstringtest DynStringTest.c)
target_link_libraries(dynstringtest 
					  dynstring
                      ${LIBS})

add_test(DynStringTestSuite DynStringTest)