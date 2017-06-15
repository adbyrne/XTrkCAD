/** \file PathsTest.c
* Unit tests for the paths module
*/

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#include "../paths.h"
#include "../layout.h"
#include <dynstring.h>


#define TESTPATH "C:\\Test\\Path"
#define TESTFILENAME "file.test"
#define TESTFILE TESTPATH "\\" TESTFILENAME
#define TESTPATH2 "D:\\Root"
#define TESTFILE2 TESTPATH2 "\\file2."

#define DEFAULTPATH "C:\\Default\\Path"

void
wPrefSetString(const char *section, const char *key, const char *value)
{}

const char *wPrefGetString(const char *section, const char *key)
{
	return(NULL);
}

const char *wGetUserHomeDir(void)
{
	return(DEFAULTPATH);
}

#include "../paths.c"

static void SetGetPath(void **state)
{
	char *string;
	(void)state;

	string = GetCurrentPath("Test");
	assert_string_equal(string, DEFAULTPATH);

	SetCurrentPath("Test", TESTFILE );
	string = GetCurrentPath("Test");
	assert_string_equal(string, TESTPATH);

	SetCurrentPath("Test", TESTFILE2);
	string = GetCurrentPath("Test");
	assert_string_equal(string, TESTPATH2);	
}

static void SetGetLayout(void **state)
{
	char *string;
	(void)state;

	SetLayoutFullPath(TESTFILE);
	string = GetLayoutFullPath();
	assert_string_equal(string, TESTFILE);

	string = GetLayoutFilename();
	assert_string_equal(string, TESTFILENAME);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
		cmocka_unit_test(SetGetPath),
		cmocka_unit_test(SetGetLayout)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}