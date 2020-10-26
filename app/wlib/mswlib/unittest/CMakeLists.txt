# build unit tests for the xtrkcad Windows library

add_executable(utf8test
  		utf8test.c
		../utf8conv.c
		)

target_link_libraries(utf8test
  		 ${LIBS})

add_test(UTF8ConversionTest utf8test)
