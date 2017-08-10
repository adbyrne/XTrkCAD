/** \file PathsTest.c
* Unit tests for the paths module
*/

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#include "common.h"

#include "../appdefaults.c"

struct appDefault tests[] = {
	{"akey"},
	{"hkey"},
	{"mkey"},
	{"zkey"}
};

#define TESTARRAYSIZE (sizeof(tests) / sizeof(tests[0]) )

struct appDefault test1[] = {
	{ "akey" }
};

#define TEST1ARRAYSIZE (sizeof(test1) / sizeof(test1[0]) )


int
wPrefGetIntegerBasic(const char *section, const char *name, long *result, long defaultValue)
{
	*result = defaultValue;
	return(TRUE);
}

int
wPrefGetFloat(const char *section, const char *name, double *result, double defaultValue)
{
	*result = defaultValue;
	return(TRUE);
}

const char * wPrefGetString(const char *section, const char *name)
{
	return(NULL);
}

static void BinarySearch(void **state)
{
	int result;
	(void)state;

	result = binarySearch(tests, 0, TESTARRAYSIZE-1, "nokey");
	assert_int_equal(result, -1);

	result = binarySearch(tests, 0, TESTARRAYSIZE-1, "akey");
	assert_int_equal(result, 0);

	result = binarySearch(tests, 1, TESTARRAYSIZE-1, "mkey");
	assert_int_equal(result, 2);

	result = binarySearch(tests, 0, TESTARRAYSIZE-1, "zkey");
	assert_int_equal(result, 3);

	result = binarySearch(test1, 0, TEST1ARRAYSIZE-1, "akey");
	assert_int_equal(result, 0);

	result = binarySearch(test1, 0, TEST1ARRAYSIZE-1, "zkey");
	assert_int_equal(result, -1);

}

static void GetDefaults(void **state)
{
	double value = 0.0;
	int intValue = 0;
	(void)state;

	wPrefGetIntegerExt("DialogItem", "cmdopt-preselect", &intValue, 2);
	assert_int_equal(intValue, 1);

	wPrefGetIntegerExt("DialogItem", "cmdopt-preselect", &intValue, 2);
	assert_int_equal(intValue, 2);

	wPrefGetIntegerExt("DialogItem", "pref-units", &intValue, 2);

}

int main(void)
{
    const struct CMUnitTest tests[] = {
		cmocka_unit_test(BinarySearch),
		cmocka_unit_test(GetDefaults)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
