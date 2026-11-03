/** \file filelinestest.c
 * Unit tests for ReadMultilineText() (filelines.c), mainly its handling of
 * input files that end before the closing END line.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <cmocka.h>

#include "common.h"
#include "fileio.h"
#include "misc.h"
#include "include/paramfile.h"
#include "include/utf8convert.h"

#define TESTFILE "filelinestest.tmp"

FILE *paramFile = NULL;
char *paramFileName = TESTFILE;
wIndex_t paramLineNum = 0;
char paramLine[STR_HUGE_SIZE];
char message[STR_HUGE_SIZE];
long paramVersion = 11;

static int noticeCnt;

int wNoticeWithIcon(int type, const char *msg, const char *yes, const char *no)
{
	(void)type;
	(void)msg;
	(void)yes;
	(void)no;
	noticeCnt++;
	return 1;
}

void ParamCheckSumLine(char *line)
{
	(void)line;
}

bool wIsUTF8(const char *string)
{
	(void)string;
	return false;
}

void ConvertUTF8ToSystem(char *in)
{
	(void)in;
}

char *MyStrdup(const char *s)
{
	return strdup(s);
}

void MyFree(void *p)
{
	free(p);
}

/** Make \p contents the current input file. */
static void openInput(const char *contents)
{
	FILE *f = fopen(TESTFILE, "w");
	assert_non_null(f);
	fputs(contents, f);
	fclose(f);
	paramFile = fopen(TESTFILE, "r");
	assert_non_null(paramFile);
	paramLine[0] = '\0';
	paramLineNum = 0;
	noticeCnt = 0;
}

static int closeInput(void **state)
{
	(void)state;
	if (paramFile) {
		fclose(paramFile);
		paramFile = NULL;
	}
	remove(TESTFILE);
	return 0;
}

static void test_reads_until_end(void **state)
{
	(void)state;
	openInput("first\nsecond\nEND\nnext object\n");

	char *text = ReadMultilineText();

	assert_string_equal(text, "first\nsecond");
	assert_int_equal(noticeCnt, 0);
	assert_non_null(paramFile);
	/* The line after END is left for the caller. */
	assert_string_equal(GetNextLine(), "next object");
	free(text);
}

static void test_no_lines_before_end(void **state)
{
	(void)state;
	openInput("END\n");

	char *text = ReadMultilineText();

	assert_string_equal(text, "");
	assert_int_equal(noticeCnt, 0);
	free(text);
}

static void test_blank_line_is_kept(void **state)
{
	(void)state;
	openInput("first\n\nthird\nEND\n");

	char *text = ReadMultilineText();

	assert_string_equal(text, "first\n\nthird");
	free(text);
}

static void test_premature_eof_keeps_text_read(void **state)
{
	(void)state;
	openInput("first\nsecond\n");

	char *text = ReadMultilineText();

	/* "second" must not be repeated: fgets() leaves the buffer alone at EOF. */
	assert_string_equal(text, "first\nsecond");
	assert_int_equal(noticeCnt, 1);
	assert_null(paramFile);
	free(text);
}

static void test_premature_eof_without_text(void **state)
{
	(void)state;
	openInput("");

	char *text = ReadMultilineText();

	assert_string_equal(text, "");
	assert_int_equal(noticeCnt, 1);
	assert_null(paramFile);
	free(text);
}

static void test_input_already_closed(void **state)
{
	(void)state;
	noticeCnt = 0;

	char *text = ReadMultilineText();

	assert_string_equal(text, "");
	assert_int_equal(noticeCnt, 0);
	free(text);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_teardown(test_reads_until_end, closeInput),
		cmocka_unit_test_teardown(test_no_lines_before_end, closeInput),
		cmocka_unit_test_teardown(test_blank_line_is_kept, closeInput),
		cmocka_unit_test_teardown(test_premature_eof_keeps_text_read, closeInput),
		cmocka_unit_test_teardown(test_premature_eof_without_text, closeInput),
		cmocka_unit_test_teardown(test_input_already_closed, closeInput),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
