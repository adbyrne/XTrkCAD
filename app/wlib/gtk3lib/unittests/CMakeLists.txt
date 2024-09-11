# build unit tests for the preferences

add_executable(preftest preftest.c)
target_link_libraries(preftest 
    PRIVATE
	xtrkcad-wlib
	${CMOCKA_LIBRARIES}
	${GTK_UNIX_PRINT3_LIBRARIES}
)
target_include_directories(preftest
	PRIVATE
	${GTK3_INCLUDE_DIRS}
	${GTK_UNIX_PRINT3_INCLUDE_DIRS}
	"../../include"
	"../../../bin"
)
add_test(PreferenceTest preftest)

set_target_properties(
	preftest 
 	PROPERTIES FOLDER UnitTestsWLib
)