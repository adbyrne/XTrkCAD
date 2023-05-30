# build unit tests for the xtrkcad Windows library

add_executable(utf8test
	"")

target_sources(utf8test
	PRIVATE	
  		utf8test.c
		../utf8conv.c
)

target_link_libraries(utf8test
	PRIVATE
		xtrkcad-wlib
		${CMOCKA_LIBRARIES}
)

add_test(UTF8ConversionTest utf8test)

set_target_properties(
	utf8test 
 	PROPERTIES FOLDER UnitTests
 )
