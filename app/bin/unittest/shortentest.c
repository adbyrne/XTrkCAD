/** \file stringutiltest.c
* Unit tests for the dxfformat module
*/

#include <malloc.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>

#include <shortentext.h>

#define RESULTSTRING "This is a test!"


static void NoRemoveBlanks(void **state)
{
	char *result = malloc(strlen(RESULTSTRING) + 20);
	(void)state;

	RemoveFormatChars(RESULTSTRING, result);
	assert_string_equal(result, RESULTSTRING);

	RemoveFormatChars(" " RESULTSTRING, result);
	assert_string_equal(result, " " RESULTSTRING);
}

static void RemoveMultipleBlanks(void **state)
{
	char *result = malloc(strlen(RESULTSTRING) + 20);
	(void)state;

	RemoveFormatChars("This  is a test!", result);
	assert_string_equal(result, RESULTSTRING);

	RemoveFormatChars("This   is a test!", result);
	assert_string_equal(result, RESULTSTRING);
}


static void RemoveTabs(void **state)
{
	char *result = malloc(strlen(RESULTSTRING) + 20);
	(void)state;

	RemoveFormatChars("This \tis\ta test!", result);
	assert_string_equal(result, RESULTSTRING);

	RemoveFormatChars("This\t\tis a\ttest!", result);
	assert_string_equal(result, RESULTSTRING);

	RemoveFormatChars("\tThis is a test!", result);
	assert_string_equal(result, " " RESULTSTRING);
}

static void RemoveCRs(void **state)
{
	char *result = malloc(strlen(RESULTSTRING) + 20);
	(void)state;

	RemoveFormatChars("This\r is a test!", result);
	assert_string_equal(result, RESULTSTRING);

	RemoveFormatChars("This\ris a test!", result);
	assert_string_equal(result, RESULTSTRING);

	RemoveFormatChars("This is a test!\r", result);
	assert_string_equal(result, RESULTSTRING);
}

static void RemoveLFs(void **state)
{
	char *result = malloc(strlen(RESULTSTRING) + 20);
	(void)state;

	RemoveFormatChars("This\n is a test!", result);
	assert_string_equal(result, RESULTSTRING);

	RemoveFormatChars("This\nis a test!", result);
	assert_string_equal(result, RESULTSTRING);

	RemoveFormatChars("\nThis is a test!", result);
	assert_string_equal(result, " " RESULTSTRING);

	RemoveFormatChars("This is a test!\r\n", result);
	assert_string_equal(result, RESULTSTRING);
}

#define LONGSTRING "The strrchr() function in C/C++ locates the last occurrence of a character in a string. It returns a pointer to the last occurrence in the string. The terminating null character is considered part of the C string. ... str : specifies the pointer to the null terminated string to be searched for."

static void NoEllipsizeText(void **state)
{
	char *result = malloc(strlen(RESULTSTRING) + 20);

	(void)state;

	EllipsizeString(RESULTSTRING, result, strlen(RESULTSTRING) + 10);
	assert_string_equal(result, RESULTSTRING);

	EllipsizeString(RESULTSTRING, NULL, strlen(RESULTSTRING) + 10);
	assert_string_equal(result, RESULTSTRING);
}

static void EllipsizeText(void **state)
{
	char *result = malloc(sizeof(LONGSTRING));
	(void)state;

	EllipsizeString(LONGSTRING, result, strlen(LONGSTRING));
	assert_string_equal(result, LONGSTRING);

	EllipsizeString(LONGSTRING, result, 40); 
	assert_string_equal(result, "The strrchr() function in C/C++...");

	EllipsizeString(LONGSTRING, result, 23);
	assert_string_equal(result, "The strrchr()...");

	strcpy(result, LONGSTRING);
	EllipsizeString(result, NULL, 23);
	assert_string_equal(result, "The strrchr()...");

}

static void LongText(void **state)
{
	char *result = malloc(sizeof(LONGSTRING));
	(void)state;

	strcpy(result, "abcdefghijklmnopqrstuvwxyz");
	EllipsizeString(result, NULL, 23);
	assert_string_equal(result, "abcdefghijklmnopqrst...");

	EllipsizeString("abcdefghijklmnopqrstuvwxyz", result, 10);
	assert_string_equal(result, "abcdefg...");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
		cmocka_unit_test(NoRemoveBlanks),
		cmocka_unit_test(RemoveMultipleBlanks),
		cmocka_unit_test(RemoveTabs),
		cmocka_unit_test(RemoveCRs),
		cmocka_unit_test(RemoveLFs),
		cmocka_unit_test(NoEllipsizeText),
		cmocka_unit_test(EllipsizeText),
		cmocka_unit_test(LongText),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}