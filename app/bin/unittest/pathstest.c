/** \file PathsTest.c
* Unit tests for the paths module
*/

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <cmocka.h>

#include <string.h>
#include <stdio.h>

#include <dynstring.h>
#include "../paths.h"

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

// Dummy functions to satisfy the linker

void
wPrefSetString(const char *section, const char *key, const char *value)
{}

char *wPrefGetStringExt(const char *section, const char *key)
{
	return(NULL);
}

char *wPrefGetString(const char *section, const char *key)
{
	return(DEFAULTPATH);
}

const char *wGetUserHomeDir(void)
{
	return(DEFAULTPATH);
}

void AbortProg(const char *a, const char *b, int d, const char *c)
{
	return;
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
		cmocka_unit_test(Makepath),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
