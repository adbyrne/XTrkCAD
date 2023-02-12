/** \file DynStringTest.c
* Unit tests for the dynstring library
*/

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#include "wlib.h"
#include "../gtkint.h"

unsigned dontHideCursor;

static void GetLibdir(void **state)
{
	(void)state;

	g_mkdir_with_parents("../share/preftest", 0777);
	const char *str1 = wGetAppLibDir();
	printf("%s\n", str1 );
	assert_non_null((void *)&str1);

	// setenv("PREFTESTLIB", ".", 1);
	// char *string = wGetAppLibDir();
	// printf("%s\n", string );
}

static void GetHomeDir(void **state)
{
	(void)state;
	const char *string = wGetUserHomeDir();
	printf("%s\n", string );
	assert_non_null((void *)&string);
}

static void StringIni(void **state)
{
	(void)state;
	char *string = wPrefGetStringBasic("hugo", "boss");
	assert_null((void *)string );

	wPrefSetString("hugo", "word", "test");

	string = wPrefGetStringBasic("hugo", "word");
	assert_string_equal("test", string);

	wPrefFlush(NULL);
}

static void IntIni(void **state)
{
	(void)state;
	long int val;

	bool res = wPrefGetIntegerBasic("hugo", "boss", &val, 2);
	assert_false(res);
	assert_int_equal(val, 2);

	// value is not an integer
	res = wPrefGetIntegerBasic("hugo", "word", &val, 1);
	assert_false(res);
	assert_int_equal(val, 1);

	// value is an integer
	wPrefSetInteger("hugo", "integer", 3);
	res = wPrefGetIntegerBasic("hugo", "integer", &val, 2);
	assert_true(res);
	assert_int_equal(val, 3);

	wPrefFlush(NULL);
}

static void FloatIni(void **state)
{
	(void)state;
	double val;

	bool res = wPrefGetFloatBasic("hugo", "boss", &val, 3.14159);
	assert_false(res);
	assert_float_equal(val, 3.14159, 0.001);

	// value is not an integer
	res = wPrefGetFloatBasic("hugo", "word", &val, 1.4142);
	assert_false(res);
	assert_float_equal(val, 1.4142, 0.001);

	// value is an integer
	wPrefSetFloat("hugo", "float", 3.14159);
	res = wPrefGetFloatBasic("hugo", "float", &val, 2.7183);
	assert_true(res);
	assert_float_equal(val, 3.14159, 0.001);

	wPrefFlush(NULL);
}



wWin_p wMain(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(GetHomeDir),
		cmocka_unit_test(GetLibdir),
		cmocka_unit_test(StringIni),
		cmocka_unit_test(IntIni),
		cmocka_unit_test(FloatIni),
	};

	wInitAppName("preftest");

	cmocka_run_group_tests(tests, NULL, NULL);

	return NULL;
}