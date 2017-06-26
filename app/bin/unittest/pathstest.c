/** \file PathsTest.c
* Unit tests for the paths module
*/

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#include <dynstring.h>
#include "../paths.h"
#include "../layout.h"

#ifdef WINDOWS
#define TESTPATH "C:\\Test\\Path"
#define TESTFILENAME "file.test"
#define TESTFILE TESTPATH "\\" TESTFILENAME
#define TESTPATH2 "D:\\Root"
#define TESTFILE2 TESTPATH2 "\\file2."

#define TESTRELATIVEPATH "Test\\Path"
#define DEFAULTPATH "C:\\Default\\Path"
#else
#define TESTPATH "/Test/Path"
#define TESTFILENAME "file.test"
#define TESTFILE TESTPATH "/" TESTFILENAME
#define TESTPATH2 "/Root"
#define TESTFILE2 TESTPATH2 "/file2."

#define TESTRELATIVEPATH "Test/Path"
#define DEFAULTPATH "/Default/Path"

#endif //WINDOWS
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

static void Makepath(void **state)
{
	(void)state;
	char *path;

#ifdef WINDOWS
		MakeFullpath(&path,
		"C:",
		TESTRELATIVEPATH,
		TESTFILENAME,
		NULL);

		assert_string_equal(path, "C:" TESTRELATIVEPATH "\\" TESTFILENAME);
#else
		MakeFullpath(&path,
		TESTRELATIVEPATH,
		TESTFILENAME,
		NULL);

		assert_string_equal(path, TESTRELATIVEPATH "/" TESTFILENAME);
#endif // WINDOWS

	free(path);

#ifdef WINDOWS
	MakeFullpath(&path,
		"C:",
		"test",
		"\\subdir",
		TESTFILENAME,
		NULL);
	assert_string_equal(path, "C:test\\subdir\\" TESTFILENAME);
#else
	MakeFullpath(&path,
		"test",
		"/subdir",
		TESTFILENAME,
		NULL);
	assert_string_equal(path, "test/subdir/" TESTFILENAME);

#endif // WINDOWS


	free(path);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
		cmocka_unit_test(SetGetPath),
		cmocka_unit_test(SetGetLayout),
		cmocka_unit_test(Makepath),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
