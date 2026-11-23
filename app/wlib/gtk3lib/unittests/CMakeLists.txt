# build unit tests for the GTK3 wlib (xtrkcad_add_unit_test(),
# CMake/XtrkcadUnitTest.cmake)

# the wlib headers and app/bin, for i18n.h
set(_xtrkcad_wlib_ut_includes
	"${CMAKE_CURRENT_SOURCE_DIR}/../../include"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../../bin"
)

# the preferences
xtrkcad_add_unit_test(preftest PreferenceTest
	FOLDER UnitTestsWLib
	INCLUDES ${_xtrkcad_wlib_ut_includes}
	LIBS xtrkcad-wlib PkgConfig::GTK3)

# the MRU list
xtrkcad_add_unit_test(mrulisttest MRUListTest
	FOLDER UnitTestsWLib
	INCLUDES ${_xtrkcad_wlib_ut_includes}
	LIBS xtrkcad-wlib PkgConfig::GTK3)

# the file selector helpers
# fileselector.c is included by the test, so xtrkcad-wlib is not linked
xtrkcad_add_unit_test(fileselectortest FileSelectorTest
	FOLDER UnitTestsWLib
	INCLUDES ${_xtrkcad_wlib_ut_includes}
	LIBS PkgConfig::GTK3)

# the getopt() replacement used on Windows
# win32/getopt.c is included by the test, so it runs on every platform
xtrkcad_add_unit_test(getopttest GetoptTest
	FOLDER UnitTestsWLib
	INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../../include")

# the Windows code page conversions (Development Issues #92)
# win32/utf8conv.c is included by the test; it calls the Windows API
if(WIN32)
	xtrkcad_add_unit_test(utf8convtest UTF8ConvTest
		FOLDER UnitTestsWLib
		INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
endif()

# positioning in multiline text controls
xtrkcad_add_unit_test(textpositiontest TextPositionTest MATH
	FOLDER UnitTestsWLib
	INCLUDES ${_xtrkcad_wlib_ut_includes}
	LIBS xtrkcad-wlib PkgConfig::GTK3)

# setting values in entry fields
# entry.c is included by the test, so xtrkcad-wlib is not linked
xtrkcad_add_unit_test(entrytest EntryTest
	FOLDER UnitTestsWLib
	INCLUDES ${_xtrkcad_wlib_ut_includes}
	LIBS PkgConfig::GTK3)

# These tests create GTK widgets, so they need a display (CI runs them under
# xvfb-run). Jobs without one, and the ASan/Valgrind jobs, where GTK's own
# allocations show up as leaks, leave them out with -LE gtk-display. Label any
# new test here that calls into GTK.
set_tests_properties(PreferenceTest MRUListTest TextPositionTest EntryTest
	PROPERTIES LABELS gtk-display
)
