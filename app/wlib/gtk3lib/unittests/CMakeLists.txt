# build unit tests for the preferences

add_executable(preftest preftest.c)
target_link_libraries(preftest 
    PRIVATE
	xtrkcad-wlib
	${CMOCKA_LIBRARIES}
	PkgConfig::GTK3
	${GTK_UNIX_PRINT3_LIBRARIES}
)
target_include_directories(preftest
	PRIVATE
	${GTK_UNIX_PRINT3_INCLUDE_DIRS}
	"../../include"
	"../../../bin"
)
add_test(PreferenceTest preftest)

set_target_properties(
	preftest
 	PROPERTIES FOLDER UnitTestsWLib
)

# build unit tests for the MRU list

add_executable(mrulisttest mrulisttest.c)
target_link_libraries(mrulisttest
    PRIVATE
	xtrkcad-wlib
	${CMOCKA_LIBRARIES}
	PkgConfig::GTK3
	${GTK_UNIX_PRINT3_LIBRARIES}
)
target_include_directories(mrulisttest
	PRIVATE
	${GTK_UNIX_PRINT3_INCLUDE_DIRS}
	"../../include"
	"../../../bin"
)
add_test(MRUListTest mrulisttest)

set_target_properties(
	mrulisttest
 	PROPERTIES FOLDER UnitTestsWLib
)

# build unit tests for the file selector helpers
# fileselector.c is included by the test, so xtrkcad-wlib is not linked

add_executable(fileselectortest fileselectortest.c)
target_link_libraries(fileselectortest
    PRIVATE
	${CMOCKA_LIBRARIES}
	PkgConfig::GTK3
)
target_include_directories(fileselectortest
	PRIVATE
	"../../include"
	"../../../bin"
)
add_test(FileSelectorTest fileselectortest)

set_target_properties(
	fileselectortest
 	PROPERTIES FOLDER UnitTestsWLib
)

# build unit tests for the getopt() replacement used on Windows
# win32/getopt.c is included by the test, so it runs on every platform

add_executable(getopttest getopttest.c)
target_link_libraries(getopttest
    PRIVATE
	${CMOCKA_LIBRARIES}
)
target_include_directories(getopttest
	PRIVATE
	"../../include"
)
add_test(GetoptTest getopttest)

set_target_properties(
	getopttest
 	PROPERTIES FOLDER UnitTestsWLib
)
